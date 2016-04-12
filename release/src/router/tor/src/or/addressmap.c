/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define ADDRESSMAP_PRIVATE

#include "or.h"
#include "addressmap.h"
#include "circuituse.h"
#include "config.h"
#include "connection_edge.h"
#include "control.h"
#include "dns.h"
#include "routerset.h"
#include "nodelist.h"

/** A client-side struct to remember requests to rewrite addresses
 * to new addresses. These structs are stored in the hash table
 * "addressmap" below.
 *
 * There are 5 ways to set an address mapping:
 * - A MapAddress command from the controller [permanent]
 * - An AddressMap directive in the torrc [permanent]
 * - When a TrackHostExits torrc directive is triggered [temporary]
 * - When a DNS resolve succeeds [temporary]
 * - When a DNS resolve fails [temporary]
 *
 * When an addressmap request is made but one is already registered,
 * the new one is replaced only if the currently registered one has
 * no "new_address" (that is, it's in the process of DNS resolve),
 * or if the new one is permanent (expires==0 or 1).
 *
 * (We overload the 'expires' field, using "0" for mappings set via
 * the configuration file, "1" for mappings set from the control
 * interface, and other values for DNS and TrackHostExit mappings that can
 * expire.)
 *
 * A mapping may be 'wildcarded'.  If "src_wildcard" is true, then
 * any address that ends with a . followed by the key for this entry will
 * get remapped by it.  If "dst_wildcard" is also true, then only the
 * matching suffix of such addresses will get replaced by new_address.
 */
typedef struct {
  char *new_address;
  time_t expires;
  addressmap_entry_source_bitfield_t source:3;
  unsigned src_wildcard:1;
  unsigned dst_wildcard:1;
  short num_resolve_failures;
} addressmap_entry_t;

/** Entry for mapping addresses to which virtual address we mapped them to. */
typedef struct {
  char *ipv4_address;
  char *ipv6_address;
  char *hostname_address;
} virtaddress_entry_t;

/** A hash table to store client-side address rewrite instructions. */
static strmap_t *addressmap=NULL;

/**
 * Table mapping addresses to which virtual address, if any, we
 * assigned them to.
 *
 * We maintain the following invariant: if [A,B] is in
 * virtaddress_reversemap, then B must be a virtual address, and [A,B]
 * must be in addressmap.  We do not require that the converse hold:
 * if it fails, then we could end up mapping two virtual addresses to
 * the same address, which is no disaster.
 **/
static strmap_t *virtaddress_reversemap=NULL;

/** Initialize addressmap. */
void
addressmap_init(void)
{
  addressmap = strmap_new();
  virtaddress_reversemap = strmap_new();
}

/** Free the memory associated with the addressmap entry <b>_ent</b>. */
static void
addressmap_ent_free(void *_ent)
{
  addressmap_entry_t *ent;
  if (!_ent)
    return;

  ent = _ent;
  tor_free(ent->new_address);
  tor_free(ent);
}

/** Free storage held by a virtaddress_entry_t* entry in <b>_ent</b>. */
static void
addressmap_virtaddress_ent_free(void *_ent)
{
  virtaddress_entry_t *ent;
  if (!_ent)
    return;

  ent = _ent;
  tor_free(ent->ipv4_address);
  tor_free(ent->ipv6_address);
  tor_free(ent->hostname_address);
  tor_free(ent);
}

/** Remove <b>address</b> (which must map to <b>ent</b>) from the
 * virtual address map. */
static void
addressmap_virtaddress_remove(const char *address, addressmap_entry_t *ent)
{
  if (ent && ent->new_address &&
      address_is_in_virtual_range(ent->new_address)) {
    virtaddress_entry_t *ve =
      strmap_get(virtaddress_reversemap, ent->new_address);
    /*log_fn(LOG_NOTICE,"remove reverse mapping for %s",ent->new_address);*/
    if (ve) {
      if (!strcmp(address, ve->ipv4_address))
        tor_free(ve->ipv4_address);
      if (!strcmp(address, ve->ipv6_address))
        tor_free(ve->ipv6_address);
      if (!strcmp(address, ve->hostname_address))
        tor_free(ve->hostname_address);
      if (!ve->ipv4_address && !ve->ipv6_address && !ve->hostname_address) {
        tor_free(ve);
        strmap_remove(virtaddress_reversemap, ent->new_address);
      }
    }
  }
}

/** Remove <b>ent</b> (which must be mapped to by <b>address</b>) from the
 * client address maps, and then free it. */
static void
addressmap_ent_remove(const char *address, addressmap_entry_t *ent)
{
  addressmap_virtaddress_remove(address, ent);
  addressmap_ent_free(ent);
}

/** Unregister all TrackHostExits mappings from any address to
 * *.exitname.exit. */
void
clear_trackexithost_mappings(const char *exitname)
{
  char *suffix = NULL;
  if (!addressmap || !exitname)
    return;
  tor_asprintf(&suffix, ".%s.exit", exitname);
  tor_strlower(suffix);

  STRMAP_FOREACH_MODIFY(addressmap, address, addressmap_entry_t *, ent) {
    if (ent->source == ADDRMAPSRC_TRACKEXIT &&
        !strcmpend(ent->new_address, suffix)) {
      addressmap_ent_remove(address, ent);
      MAP_DEL_CURRENT(address);
    }
  } STRMAP_FOREACH_END;

  tor_free(suffix);
}

/** Remove all TRACKEXIT mappings from the addressmap for which the target
 * host is unknown or no longer allowed, or for which the source address
 * is no longer in trackexithosts. */
void
addressmap_clear_excluded_trackexithosts(const or_options_t *options)
{
  const routerset_t *allow_nodes = options->ExitNodes;
  const routerset_t *exclude_nodes = options->ExcludeExitNodesUnion_;

  if (!addressmap)
    return;
  if (routerset_is_empty(allow_nodes))
    allow_nodes = NULL;
  if (allow_nodes == NULL && routerset_is_empty(exclude_nodes))
    return;

  STRMAP_FOREACH_MODIFY(addressmap, address, addressmap_entry_t *, ent) {
    size_t len;
    const char *target = ent->new_address, *dot;
    char *nodename;
    const node_t *node;

    if (!target) {
      /* DNS resolving in progress */
      continue;
    } else if (strcmpend(target, ".exit")) {
      /* Not a .exit mapping */
      continue;
    } else if (ent->source != ADDRMAPSRC_TRACKEXIT) {
      /* Not a trackexit mapping. */
      continue;
    }
    len = strlen(target);
    if (len < 6)
      continue; /* malformed. */
    dot = target + len - 6; /* dot now points to just before .exit */
    while (dot > target && *dot != '.')
      dot--;
    if (*dot == '.') dot++;
    nodename = tor_strndup(dot, len-5-(dot-target));;
    node = node_get_by_nickname(nodename, 0);
    tor_free(nodename);
    if (!node ||
        (allow_nodes && !routerset_contains_node(allow_nodes, node)) ||
        routerset_contains_node(exclude_nodes, node) ||
        !hostname_in_track_host_exits(options, address)) {
      /* We don't know this one, or we want to be rid of it. */
      addressmap_ent_remove(address, ent);
      MAP_DEL_CURRENT(address);
    }
  } STRMAP_FOREACH_END;
}

/** Return true iff <b>address</b> is one that we are configured to
 * automap on resolve according to <b>options</b>. */
int
addressmap_address_should_automap(const char *address,
                                  const or_options_t *options)
{
  const smartlist_t *suffix_list = options->AutomapHostsSuffixes;

  if (!suffix_list)
    return 0;

  SMARTLIST_FOREACH_BEGIN(suffix_list, const char *, suffix) {
    if (!strcmp(suffix, "."))
      return 1;
    if (!strcasecmpend(address, suffix))
      return 1;
  } SMARTLIST_FOREACH_END(suffix);
  return 0;
}

/** Remove all AUTOMAP mappings from the addressmap for which the
 * source address no longer matches AutomapHostsSuffixes, which is
 * no longer allowed by AutomapHostsOnResolve, or for which the
 * target address is no longer in the virtual network. */
void
addressmap_clear_invalid_automaps(const or_options_t *options)
{
  int clear_all = !options->AutomapHostsOnResolve;
  const smartlist_t *suffixes = options->AutomapHostsSuffixes;

  if (!addressmap)
    return;

  if (!suffixes)
    clear_all = 1; /* This should be impossible, but let's be sure. */

  STRMAP_FOREACH_MODIFY(addressmap, src_address, addressmap_entry_t *, ent) {
    int remove = clear_all;
    if (ent->source != ADDRMAPSRC_AUTOMAP)
      continue; /* not an automap mapping. */

    if (!remove) {
      remove = ! addressmap_address_should_automap(src_address, options);
    }

    if (!remove && ! address_is_in_virtual_range(ent->new_address))
      remove = 1;

    if (remove) {
      addressmap_ent_remove(src_address, ent);
      MAP_DEL_CURRENT(src_address);
    }
  } STRMAP_FOREACH_END;
}

/** Remove all entries from the addressmap that were set via the
 * configuration file or the command line. */
void
addressmap_clear_configured(void)
{
  addressmap_get_mappings(NULL, 0, 0, 0);
}

/** Remove all entries from the addressmap that are set to expire, ever. */
void
addressmap_clear_transient(void)
{
  addressmap_get_mappings(NULL, 2, TIME_MAX, 0);
}

/** Clean out entries from the addressmap cache that were
 * added long enough ago that they are no longer valid.
 */
void
addressmap_clean(time_t now)
{
  addressmap_get_mappings(NULL, 2, now, 0);
}

/** Free all the elements in the addressmap, and free the addressmap
 * itself. */
void
addressmap_free_all(void)
{
  strmap_free(addressmap, addressmap_ent_free);
  addressmap = NULL;

  strmap_free(virtaddress_reversemap, addressmap_virtaddress_ent_free);
  virtaddress_reversemap = NULL;
}

/** Try to find a match for AddressMap expressions that use
 *  wildcard notation such as '*.c.d *.e.f' (so 'a.c.d' will map to 'a.e.f') or
 *  '*.c.d a.b.c' (so 'a.c.d' will map to a.b.c).
 *  Return the matching entry in AddressMap or NULL if no match is found.
 *  For expressions such as '*.c.d *.e.f', truncate <b>address</b> 'a.c.d'
 *  to 'a' before we return the matching AddressMap entry.
 *
 * This function does not handle the case where a pattern of the form "*.c.d"
 * matches the address c.d -- that's done by the main addressmap_rewrite
 * function.
 */
static addressmap_entry_t *
addressmap_match_superdomains(char *address)
{
  addressmap_entry_t *val;
  char *cp;

  cp = address;
  while ((cp = strchr(cp, '.'))) {
    /* cp now points to a suffix of address that begins with a . */
    val = strmap_get_lc(addressmap, cp+1);
    if (val && val->src_wildcard) {
      if (val->dst_wildcard)
        *cp = '\0';
      return val;
    }
    ++cp;
  }
  return NULL;
}

/** Look at address, and rewrite it until it doesn't want any
 * more rewrites; but don't get into an infinite loop.
 * Don't write more than maxlen chars into address.  Return true if the
 * address changed; false otherwise.  Set *<b>expires_out</b> to the
 * expiry time of the result, or to <b>time_max</b> if the result does
 * not expire.
 *
 * If <b>exit_source_out</b> is non-null, we set it as follows.  If we the
 * address starts out as a non-exit address, and we remap it to an .exit
 * address at any point, then set *<b>exit_source_out</b> to the
 * address_entry_source_t of the first such rule.  Set *<b>exit_source_out</b>
 * to ADDRMAPSRC_NONE if there is no such rewrite, or if the original address
 * was a .exit.
 */
int
addressmap_rewrite(char *address, size_t maxlen,
                   unsigned flags,
                   time_t *expires_out,
                   addressmap_entry_source_t *exit_source_out)
{
  addressmap_entry_t *ent;
  int rewrites;
  time_t expires = TIME_MAX;
  addressmap_entry_source_t exit_source = ADDRMAPSRC_NONE;
  char *addr_orig = tor_strdup(address);
  char *log_addr_orig = NULL;

  for (rewrites = 0; rewrites < 16; rewrites++) {
    int exact_match = 0;
    log_addr_orig = tor_strdup(escaped_safe_str_client(address));

    ent = strmap_get(addressmap, address);

    if (!ent || !ent->new_address) {
      ent = addressmap_match_superdomains(address);
    } else {
      if (ent->src_wildcard && !ent->dst_wildcard &&
          !strcasecmp(address, ent->new_address)) {
        /* This is a rule like *.example.com example.com, and we just got
         * "example.com" */
        goto done;
      }

      exact_match = 1;
    }

    if (!ent || !ent->new_address) {
      goto done;
    }

    switch (ent->source) {
      case ADDRMAPSRC_DNS:
        {
          sa_family_t f;
          tor_addr_t tmp;
          f = tor_addr_parse(&tmp, ent->new_address);
          if (f == AF_INET && !(flags & AMR_FLAG_USE_IPV4_DNS))
            goto done;
          else if (f == AF_INET6 && !(flags & AMR_FLAG_USE_IPV6_DNS))
            goto done;
        }
        break;
      case ADDRMAPSRC_CONTROLLER:
      case ADDRMAPSRC_TORRC:
        if (!(flags & AMR_FLAG_USE_MAPADDRESS))
          goto done;
        break;
      case ADDRMAPSRC_AUTOMAP:
        if (!(flags & AMR_FLAG_USE_AUTOMAP))
          goto done;
        break;
      case ADDRMAPSRC_TRACKEXIT:
        if (!(flags & AMR_FLAG_USE_TRACKEXIT))
          goto done;
        break;
      case ADDRMAPSRC_NONE:
      default:
        log_warn(LD_BUG, "Unknown addrmap source value %d. Ignoring it.",
                 (int) ent->source);
        goto done;
    }

    if (ent->dst_wildcard && !exact_match) {
      strlcat(address, ".", maxlen);
      strlcat(address, ent->new_address, maxlen);
    } else {
      strlcpy(address, ent->new_address, maxlen);
    }

    if (!strcmpend(address, ".exit") &&
        strcmpend(addr_orig, ".exit") &&
        exit_source == ADDRMAPSRC_NONE) {
      exit_source = ent->source;
    }

    log_info(LD_APP, "Addressmap: rewriting %s to %s",
             log_addr_orig, escaped_safe_str_client(address));
    if (ent->expires > 1 && ent->expires < expires)
      expires = ent->expires;

    tor_free(log_addr_orig);
  }
  log_warn(LD_CONFIG,
           "Loop detected: we've rewritten %s 16 times! Using it as-is.",
           escaped_safe_str_client(address));
  /* it's fine to rewrite a rewrite, but don't loop forever */

 done:
  tor_free(addr_orig);
  tor_free(log_addr_orig);
  if (exit_source_out)
    *exit_source_out = exit_source;
  if (expires_out)
    *expires_out = expires;
  return (rewrites > 0);
}

/** If we have a cached reverse DNS entry for the address stored in the
 * <b>maxlen</b>-byte buffer <b>address</b> (typically, a dotted quad) then
 * rewrite to the cached value and return 1.  Otherwise return 0.  Set
 * *<b>expires_out</b> to the expiry time of the result, or to <b>time_max</b>
 * if the result does not expire. */
int
addressmap_rewrite_reverse(char *address, size_t maxlen, unsigned flags,
                           time_t *expires_out)
{
  char *s, *cp;
  addressmap_entry_t *ent;
  int r = 0;
  {
    sa_family_t f;
    tor_addr_t tmp;
    f = tor_addr_parse(&tmp, address);
    if (f == AF_INET && !(flags & AMR_FLAG_USE_IPV4_DNS))
      return 0;
    else if (f == AF_INET6 && !(flags & AMR_FLAG_USE_IPV6_DNS))
      return 0;
    /* FFFF we should reverse-map virtual addresses even if we haven't
     * enabled DNS cacheing. */
  }

  tor_asprintf(&s, "REVERSE[%s]", address);
  ent = strmap_get(addressmap, s);
  if (ent) {
    cp = tor_strdup(escaped_safe_str_client(ent->new_address));
    log_info(LD_APP, "Rewrote reverse lookup %s -> %s",
             escaped_safe_str_client(s), cp);
    tor_free(cp);
    strlcpy(address, ent->new_address, maxlen);
    r = 1;
  }

  if (expires_out)
    *expires_out = (ent && ent->expires > 1) ? ent->expires : TIME_MAX;

  tor_free(s);
  return r;
}

/** Return 1 if <b>address</b> is already registered, else return 0. If address
 * is already registered, and <b>update_expires</b> is non-zero, then update
 * the expiry time on the mapping with update_expires if it is a
 * mapping created by TrackHostExits. */
int
addressmap_have_mapping(const char *address, int update_expiry)
{
  addressmap_entry_t *ent;
  if (!(ent=strmap_get_lc(addressmap, address)))
    return 0;
  if (update_expiry && ent->source==ADDRMAPSRC_TRACKEXIT)
    ent->expires=time(NULL) + update_expiry;
  return 1;
}

/** Register a request to map <b>address</b> to <b>new_address</b>,
 * which will expire on <b>expires</b> (or 0 if never expires from
 * config file, 1 if never expires from controller, 2 if never expires
 * (virtual address mapping) from the controller.)
 *
 * <b>new_address</b> should be a newly dup'ed string, which we'll use or
 * free as appropriate. We will leave address alone.
 *
 * If <b>wildcard_addr</b> is true, then the mapping will match any address
 * equal to <b>address</b>, or any address ending with a period followed by
 * <b>address</b>.  If <b>wildcard_addr</b> and <b>wildcard_new_addr</b> are
 * both true, the mapping will rewrite addresses that end with
 * ".<b>address</b>" into ones that end with ".<b>new_address</b>".
 *
 * If <b>new_address</b> is NULL, or <b>new_address</b> is equal to
 * <b>address</b> and <b>wildcard_addr</b> is equal to
 * <b>wildcard_new_addr</b>, remove any mappings that exist from
 * <b>address</b>.
 *
 *
 * It is an error to set <b>wildcard_new_addr</b> if <b>wildcard_addr</b> is
 * not set. */
void
addressmap_register(const char *address, char *new_address, time_t expires,
                    addressmap_entry_source_t source,
                    const int wildcard_addr,
                    const int wildcard_new_addr)
{
  addressmap_entry_t *ent;

  if (wildcard_new_addr)
    tor_assert(wildcard_addr);

  ent = strmap_get(addressmap, address);
  if (!new_address || (!strcasecmp(address,new_address) &&
                       wildcard_addr == wildcard_new_addr)) {
    /* Remove the mapping, if any. */
    tor_free(new_address);
    if (ent) {
      addressmap_ent_remove(address,ent);
      strmap_remove(addressmap, address);
    }
    return;
  }
  if (!ent) { /* make a new one and register it */
    ent = tor_malloc_zero(sizeof(addressmap_entry_t));
    strmap_set(addressmap, address, ent);
  } else if (ent->new_address) { /* we need to clean up the old mapping. */
    if (expires > 1) {
      log_info(LD_APP,"Temporary addressmap ('%s' to '%s') not performed, "
               "since it's already mapped to '%s'",
               safe_str_client(address),
               safe_str_client(new_address),
               safe_str_client(ent->new_address));
      tor_free(new_address);
      return;
    }
    if (address_is_in_virtual_range(ent->new_address) &&
        expires != 2) {
      /* XXX This isn't the perfect test; we want to avoid removing
       * mappings set from the control interface _as virtual mapping */
      addressmap_virtaddress_remove(address, ent);
    }
    tor_free(ent->new_address);
  } /* else { we have an in-progress resolve with no mapping. } */

  ent->new_address = new_address;
  ent->expires = expires==2 ? 1 : expires;
  ent->num_resolve_failures = 0;
  ent->source = source;
  ent->src_wildcard = wildcard_addr ? 1 : 0;
  ent->dst_wildcard = wildcard_new_addr ? 1 : 0;

  log_info(LD_CONFIG, "Addressmap: (re)mapped '%s' to '%s'",
           safe_str_client(address),
           safe_str_client(ent->new_address));
  control_event_address_mapped(address, ent->new_address, expires, NULL, 1);
}

/** An attempt to resolve <b>address</b> failed at some OR.
 * Increment the number of resolve failures we have on record
 * for it, and then return that number.
 */
int
client_dns_incr_failures(const char *address)
{
  addressmap_entry_t *ent = strmap_get(addressmap, address);
  if (!ent) {
    ent = tor_malloc_zero(sizeof(addressmap_entry_t));
    ent->expires = time(NULL) + MAX_DNS_ENTRY_AGE;
    strmap_set(addressmap,address,ent);
  }
  if (ent->num_resolve_failures < SHORT_MAX)
    ++ent->num_resolve_failures; /* don't overflow */
  log_info(LD_APP, "Address %s now has %d resolve failures.",
           safe_str_client(address),
           ent->num_resolve_failures);
  return ent->num_resolve_failures;
}

/** If <b>address</b> is in the client DNS addressmap, reset
 * the number of resolve failures we have on record for it.
 * This is used when we fail a stream because it won't resolve:
 * otherwise future attempts on that address will only try once.
 */
void
client_dns_clear_failures(const char *address)
{
  addressmap_entry_t *ent = strmap_get(addressmap, address);
  if (ent)
    ent->num_resolve_failures = 0;
}

/** Record the fact that <b>address</b> resolved to <b>name</b>.
 * We can now use this in subsequent streams via addressmap_rewrite()
 * so we can more correctly choose an exit that will allow <b>address</b>.
 *
 * If <b>exitname</b> is defined, then append the addresses with
 * ".exitname.exit" before registering the mapping.
 *
 * If <b>ttl</b> is nonnegative, the mapping will be valid for
 * <b>ttl</b>seconds; otherwise, we use the default.
 */
static void
client_dns_set_addressmap_impl(entry_connection_t *for_conn,
                               const char *address, const char *name,
                               const char *exitname,
                               int ttl)
{
  char *extendedaddress=NULL, *extendedval=NULL;
  (void)for_conn;

  tor_assert(address);
  tor_assert(name);

  if (ttl<0)
    ttl = DEFAULT_DNS_TTL;
  else
    ttl = dns_clip_ttl(ttl);

  if (exitname) {
    /* XXXX fails to ever get attempts to get an exit address of
     * google.com.digest[=~]nickname.exit; we need a syntax for this that
     * won't make strict RFC952-compliant applications (like us) barf. */
    tor_asprintf(&extendedaddress,
                 "%s.%s.exit", address, exitname);
    tor_asprintf(&extendedval,
                 "%s.%s.exit", name, exitname);
  } else {
    tor_asprintf(&extendedaddress,
                 "%s", address);
    tor_asprintf(&extendedval,
                 "%s", name);
  }
  addressmap_register(extendedaddress, extendedval,
                      time(NULL) + ttl, ADDRMAPSRC_DNS, 0, 0);
  tor_free(extendedaddress);
}

/** Record the fact that <b>address</b> resolved to <b>val</b>.
 * We can now use this in subsequent streams via addressmap_rewrite()
 * so we can more correctly choose an exit that will allow <b>address</b>.
 *
 * If <b>exitname</b> is defined, then append the addresses with
 * ".exitname.exit" before registering the mapping.
 *
 * If <b>ttl</b> is nonnegative, the mapping will be valid for
 * <b>ttl</b>seconds; otherwise, we use the default.
 */
void
client_dns_set_addressmap(entry_connection_t *for_conn,
                          const char *address,
                          const tor_addr_t *val,
                          const char *exitname,
                          int ttl)
{
  tor_addr_t addr_tmp;
  char valbuf[TOR_ADDR_BUF_LEN];

  tor_assert(address);
  tor_assert(val);

  if (tor_addr_parse(&addr_tmp, address) >= 0)
    return; /* If address was an IP address already, don't add a mapping. */

  if (tor_addr_family(val) == AF_INET) {
    if (! for_conn->entry_cfg.cache_ipv4_answers)
      return;
  } else if (tor_addr_family(val) == AF_INET6) {
    if (! for_conn->entry_cfg.cache_ipv6_answers)
      return;
  }

  if (! tor_addr_to_str(valbuf, val, sizeof(valbuf), 1))
    return;

  client_dns_set_addressmap_impl(for_conn, address, valbuf, exitname, ttl);
}

/** Add a cache entry noting that <b>address</b> (ordinarily a dotted quad)
 * resolved via a RESOLVE_PTR request to the hostname <b>v</b>.
 *
 * If <b>exitname</b> is defined, then append the addresses with
 * ".exitname.exit" before registering the mapping.
 *
 * If <b>ttl</b> is nonnegative, the mapping will be valid for
 * <b>ttl</b>seconds; otherwise, we use the default.
 */
void
client_dns_set_reverse_addressmap(entry_connection_t *for_conn,
                                  const char *address, const char *v,
                                  const char *exitname,
                                  int ttl)
{
  char *s = NULL;
  {
    tor_addr_t tmp_addr;
    sa_family_t f = tor_addr_parse(&tmp_addr, address);
    if ((f == AF_INET && ! for_conn->entry_cfg.cache_ipv4_answers) ||
        (f == AF_INET6 && ! for_conn->entry_cfg.cache_ipv6_answers))
      return;
  }
  tor_asprintf(&s, "REVERSE[%s]", address);
  client_dns_set_addressmap_impl(for_conn, s, v, exitname, ttl);
  tor_free(s);
}

/* By default, we hand out 127.192.0.1 through 127.254.254.254.
 * These addresses should map to localhost, so even if the
 * application accidentally tried to connect to them directly (not
 * via Tor), it wouldn't get too far astray.
 *
 * These options are configured by parse_virtual_addr_network().
 */

static virtual_addr_conf_t virtaddr_conf_ipv4;
static virtual_addr_conf_t virtaddr_conf_ipv6;

/** Read a netmask of the form 127.192.0.0/10 from "val", and check whether
 * it's a valid set of virtual addresses to hand out in response to MAPADDRESS
 * requests.  Return 0 on success; set *msg (if provided) to a newly allocated
 * string and return -1 on failure.  If validate_only is false, sets the
 * actual virtual address range to the parsed value. */
int
parse_virtual_addr_network(const char *val, sa_family_t family,
                           int validate_only,
                           char **msg)
{
  const int ipv6 = (family == AF_INET6);
  tor_addr_t addr;
  maskbits_t bits;
  const int max_bits = ipv6 ? 40 : 16;
  virtual_addr_conf_t *conf = ipv6 ? &virtaddr_conf_ipv6 : &virtaddr_conf_ipv4;

  if (!val || val[0] == '\0') {
    if (msg)
      tor_asprintf(msg, "Value not present (%s) after VirtualAddressNetwork%s",
                   val?"Empty":"NULL", ipv6?"IPv6":"");
    return -1;
  }
  if (tor_addr_parse_mask_ports(val, 0, &addr, &bits, NULL, NULL) < 0) {
    if (msg)
      tor_asprintf(msg, "Error parsing VirtualAddressNetwork%s %s",
                   ipv6?"IPv6":"", val);
    return -1;
  }
  if (tor_addr_family(&addr) != family) {
    if (msg)
      tor_asprintf(msg, "Incorrect address type for VirtualAddressNetwork%s",
                   ipv6?"IPv6":"");
    return -1;
  }
#if 0
  if (port_min != 1 || port_max != 65535) {
    if (msg)
      tor_asprintf(msg, "Can't specify ports on VirtualAddressNetwork%s",
                   ipv6?"IPv6":"");
    return -1;
  }
#endif

  if (bits > max_bits) {
    if (msg)
      tor_asprintf(msg, "VirtualAddressNetwork%s expects a /%d "
                   "network or larger",ipv6?"IPv6":"", max_bits);
    return -1;
  }

  if (validate_only)
    return 0;

  tor_addr_copy(&conf->addr, &addr);
  conf->bits = bits;

  return 0;
}

/**
 * Return true iff <b>addr</b> is likely to have been returned by
 * client_dns_get_unused_address.
 **/
int
address_is_in_virtual_range(const char *address)
{
  tor_addr_t addr;
  tor_assert(address);
  if (!strcasecmpend(address, ".virtual")) {
    return 1;
  } else if (tor_addr_parse(&addr, address) >= 0) {
    const virtual_addr_conf_t *conf = (tor_addr_family(&addr) == AF_INET6) ?
      &virtaddr_conf_ipv6 : &virtaddr_conf_ipv4;
    if (tor_addr_compare_masked(&addr, &conf->addr, conf->bits, CMP_EXACT)==0)
      return 1;
  }
  return 0;
}

/** Return a random address conforming to the virtual address configuration
 * in <b>conf</b>.
 */
STATIC void
get_random_virtual_addr(const virtual_addr_conf_t *conf, tor_addr_t *addr_out)
{
  uint8_t tmp[4];
  const uint8_t *addr_bytes;
  uint8_t bytes[16];
  const int ipv6 = tor_addr_family(&conf->addr) == AF_INET6;
  const int total_bytes = ipv6 ? 16 : 4;

  tor_assert(conf->bits <= total_bytes * 8);

  /* Set addr_bytes to the bytes of the virtual network, in host order */
  if (ipv6) {
    addr_bytes = tor_addr_to_in6_addr8(&conf->addr);
  } else {
    set_uint32(tmp, tor_addr_to_ipv4n(&conf->addr));
    addr_bytes = tmp;
  }

  /* Get an appropriate number of random bytes. */
  crypto_rand((char*)bytes, total_bytes);

  /* Now replace the first "conf->bits" bits of 'bytes' with addr_bytes*/
  if (conf->bits >= 8)
    memcpy(bytes, addr_bytes, conf->bits / 8);
  if (conf->bits & 7) {
    uint8_t mask = 0xff >> (conf->bits & 7);
    bytes[conf->bits/8] &= mask;
    bytes[conf->bits/8] |= addr_bytes[conf->bits/8] & ~mask;
  }

  if (ipv6)
    tor_addr_from_ipv6_bytes(addr_out, (char*) bytes);
  else
    tor_addr_from_ipv4n(addr_out, get_uint32(bytes));

  tor_assert(tor_addr_compare_masked(addr_out, &conf->addr,
                                     conf->bits, CMP_EXACT)==0);
}

/** Return a newly allocated string holding an address of <b>type</b>
 * (one of RESOLVED_TYPE_{IPV4|IPV6|HOSTNAME}) that has not yet been
 * mapped, and that is very unlikely to be the address of any real host.
 *
 * May return NULL if we have run out of virtual addresses.
 */
static char *
addressmap_get_virtual_address(int type)
{
  char buf[64];
  tor_assert(addressmap);

  if (type == RESOLVED_TYPE_HOSTNAME) {
    char rand[10];
    do {
      crypto_rand(rand, sizeof(rand));
      base32_encode(buf,sizeof(buf),rand,sizeof(rand));
      strlcat(buf, ".virtual", sizeof(buf));
    } while (strmap_get(addressmap, buf));
    return tor_strdup(buf);
  } else if (type == RESOLVED_TYPE_IPV4 || type == RESOLVED_TYPE_IPV6) {
    const int ipv6 = (type == RESOLVED_TYPE_IPV6);
    const virtual_addr_conf_t *conf = ipv6 ?
      &virtaddr_conf_ipv6 : &virtaddr_conf_ipv4;

    /* Don't try more than 1000 times.  This gives us P < 1e-9 for
     * failing to get a good address so long as the address space is
     * less than ~97.95% full.  That's always going to be true under
     * sensible circumstances for an IPv6 /10, and it's going to be
     * true for an IPv4 /10 as long as we've handed out less than
     * 4.08 million addresses. */
    uint32_t attempts = 1000;

    tor_addr_t addr;

    while (attempts--) {
      get_random_virtual_addr(conf, &addr);

      if (!ipv6) {
        /* Don't hand out any .0 or .255 address. */
        const uint32_t a = tor_addr_to_ipv4h(&addr);
        if ((a & 0xff) == 0 || (a & 0xff) == 0xff)
          continue;
      }

      tor_addr_to_str(buf, &addr, sizeof(buf), 1);
      if (!strmap_get(addressmap, buf)) {
        /* XXXX This code is to make sure I didn't add an undecorated version
         * by mistake. I hope it's needless. */
        char tmp[TOR_ADDR_BUF_LEN];
        tor_addr_to_str(tmp, &addr, sizeof(tmp), 0);
        if (strmap_get(addressmap, tmp)) {
          log_warn(LD_BUG, "%s wasn't in the addressmap, but %s was.",
                   buf, tmp);
          continue;
        }

        return tor_strdup(buf);
      }
    }
    log_warn(LD_CONFIG, "Ran out of virtual addresses!");
    return NULL;
  } else {
    log_warn(LD_BUG, "Called with unsupported address type (%d)", type);
    return NULL;
  }
}

/** A controller has requested that we map some address of type
 * <b>type</b> to the address <b>new_address</b>.  Choose an address
 * that is unlikely to be used, and map it, and return it in a newly
 * allocated string.  If another address of the same type is already
 * mapped to <b>new_address</b>, try to return a copy of that address.
 *
 * The string in <b>new_address</b> may be freed or inserted into a map
 * as appropriate.  May return NULL if are out of virtual addresses.
 **/
const char *
addressmap_register_virtual_address(int type, char *new_address)
{
  char **addrp;
  virtaddress_entry_t *vent;
  int vent_needs_to_be_added = 0;

  tor_assert(new_address);
  tor_assert(addressmap);
  tor_assert(virtaddress_reversemap);

  vent = strmap_get(virtaddress_reversemap, new_address);
  if (!vent) {
    vent = tor_malloc_zero(sizeof(virtaddress_entry_t));
    vent_needs_to_be_added = 1;
  }

  if (type == RESOLVED_TYPE_IPV4)
    addrp = &vent->ipv4_address;
  else if (type == RESOLVED_TYPE_IPV6)
    addrp = &vent->ipv6_address;
  else
    addrp = &vent->hostname_address;

  if (*addrp) {
    addressmap_entry_t *ent = strmap_get(addressmap, *addrp);
    if (ent && ent->new_address &&
        !strcasecmp(new_address, ent->new_address)) {
      tor_free(new_address);
      tor_assert(!vent_needs_to_be_added);
      return *addrp;
    } else {
      log_warn(LD_BUG,
               "Internal confusion: I thought that '%s' was mapped to by "
               "'%s', but '%s' really maps to '%s'. This is a harmless bug.",
               safe_str_client(new_address),
               safe_str_client(*addrp),
               safe_str_client(*addrp),
               ent?safe_str_client(ent->new_address):"(nothing)");
    }
  }

  tor_free(*addrp);
  *addrp = addressmap_get_virtual_address(type);
  if (!*addrp) {
    tor_free(vent);
    tor_free(new_address);
    return NULL;
  }
  log_info(LD_APP, "Registering map from %s to %s", *addrp, new_address);
  if (vent_needs_to_be_added)
    strmap_set(virtaddress_reversemap, new_address, vent);
  addressmap_register(*addrp, new_address, 2, ADDRMAPSRC_AUTOMAP, 0, 0);

  /* FFFF register corresponding reverse mapping. */

#if 0
  {
    /* Try to catch possible bugs */
    addressmap_entry_t *ent;
    ent = strmap_get(addressmap, *addrp);
    tor_assert(ent);
    tor_assert(!strcasecmp(ent->new_address,new_address));
    vent = strmap_get(virtaddress_reversemap, new_address);
    tor_assert(vent);
    tor_assert(!strcasecmp(*addrp,
                           (type == RESOLVED_TYPE_IPV4) ?
                           vent->ipv4_address : vent->hostname_address));
    log_info(LD_APP, "Map from %s to %s okay.",
             safe_str_client(*addrp),
             safe_str_client(new_address));
  }
#endif

  return *addrp;
}

/** Return 1 if <b>address</b> has funny characters in it like colons. Return
 * 0 if it's fine, or if we're configured to allow it anyway.  <b>client</b>
 * should be true if we're using this address as a client; false if we're
 * using it as a server.
 */
int
address_is_invalid_destination(const char *address, int client)
{
  if (client) {
    if (get_options()->AllowNonRFC953Hostnames)
      return 0;
  } else {
    if (get_options()->ServerDNSAllowNonRFC953Hostnames)
      return 0;
  }

  /* It might be an IPv6 address! */
  {
    tor_addr_t a;
    if (tor_addr_parse(&a, address) >= 0)
      return 0;
  }

  while (*address) {
    if (TOR_ISALNUM(*address) ||
        *address == '-' ||
        *address == '.' ||
        *address == '_') /* Underscore is not allowed, but Windows does it
                          * sometimes, just to thumb its nose at the IETF. */
      ++address;
    else
      return 1;
  }
  return 0;
}

/** Iterate over all address mappings which have expiry times between
 * min_expires and max_expires, inclusive.  If sl is provided, add an
 * "old-addr new-addr expiry" string to sl for each mapping, omitting
 * the expiry time if want_expiry is false. If sl is NULL, remove the
 * mappings.
 */
void
addressmap_get_mappings(smartlist_t *sl, time_t min_expires,
                        time_t max_expires, int want_expiry)
{
   strmap_iter_t *iter;
   const char *key;
   void *val_;
   addressmap_entry_t *val;

   if (!addressmap)
     addressmap_init();

   for (iter = strmap_iter_init(addressmap); !strmap_iter_done(iter); ) {
     strmap_iter_get(iter, &key, &val_);
     val = val_;
     if (val->expires >= min_expires && val->expires <= max_expires) {
       if (!sl) {
         iter = strmap_iter_next_rmv(addressmap,iter);
         addressmap_ent_remove(key, val);
         continue;
       } else if (val->new_address) {
         const char *src_wc = val->src_wildcard ? "*." : "";
         const char *dst_wc = val->dst_wildcard ? "*." : "";
         if (want_expiry) {
           if (val->expires < 3 || val->expires == TIME_MAX)
             smartlist_add_asprintf(sl, "%s%s %s%s NEVER",
                                    src_wc, key, dst_wc, val->new_address);
           else {
             char time[ISO_TIME_LEN+1];
             format_iso_time(time, val->expires);
             smartlist_add_asprintf(sl, "%s%s %s%s \"%s\"",
                                    src_wc, key, dst_wc, val->new_address,
                                    time);
           }
         } else {
           smartlist_add_asprintf(sl, "%s%s %s%s",
                                  src_wc, key, dst_wc, val->new_address);
         }
       }
     }
     iter = strmap_iter_next(addressmap,iter);
   }
}

