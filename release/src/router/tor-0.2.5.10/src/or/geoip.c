/* Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file geoip.c
 * \brief Functions related to maintaining an IP-to-country database;
 * to summarizing client connections by country to entry guards, bridges,
 * and directory servers; and for statistics on answering network status
 * requests.
 */

#define GEOIP_PRIVATE
#include "or.h"
#include "ht.h"
#include "config.h"
#include "control.h"
#include "dnsserv.h"
#include "geoip.h"
#include "routerlist.h"

static void clear_geoip_db(void);
static void init_geoip_countries(void);

/** An entry from the GeoIP IPv4 file: maps an IPv4 range to a country. */
typedef struct geoip_ipv4_entry_t {
  uint32_t ip_low; /**< The lowest IP in the range, in host order */
  uint32_t ip_high; /**< The highest IP in the range, in host order */
  intptr_t country; /**< An index into geoip_countries */
} geoip_ipv4_entry_t;

/** An entry from the GeoIP IPv6 file: maps an IPv6 range to a country. */
typedef struct geoip_ipv6_entry_t {
  struct in6_addr ip_low; /**< The lowest IP in the range, in host order */
  struct in6_addr ip_high; /**< The highest IP in the range, in host order */
  intptr_t country; /**< An index into geoip_countries */
} geoip_ipv6_entry_t;

/** A per-country record for GeoIP request history. */
typedef struct geoip_country_t {
  char countrycode[3];
  uint32_t n_v3_ns_requests;
} geoip_country_t;

/** A list of geoip_country_t */
static smartlist_t *geoip_countries = NULL;
/** A map from lowercased country codes to their position in geoip_countries.
 * The index is encoded in the pointer, and 1 is added so that NULL can mean
 * not found. */
static strmap_t *country_idxplus1_by_lc_code = NULL;
/** Lists of all known geoip_ipv4_entry_t and geoip_ipv6_entry_t, sorted
 * by their respective ip_low. */
static smartlist_t *geoip_ipv4_entries = NULL, *geoip_ipv6_entries = NULL;

/** SHA1 digest of the GeoIP files to include in extra-info descriptors. */
static char geoip_digest[DIGEST_LEN];
static char geoip6_digest[DIGEST_LEN];

/** Return the index of the <b>country</b>'s entry in the GeoIP
 * country list if it is a valid 2-letter country code, otherwise
 * return -1. */
country_t
geoip_get_country(const char *country)
{
  void *idxplus1_;
  intptr_t idx;

  idxplus1_ = strmap_get_lc(country_idxplus1_by_lc_code, country);
  if (!idxplus1_)
    return -1;

  idx = ((uintptr_t)idxplus1_)-1;
  return (country_t)idx;
}

/** Add an entry to a GeoIP table, mapping all IP addresses between <b>low</b>
 * and <b>high</b>, inclusive, to the 2-letter country code <b>country</b>. */
static void
geoip_add_entry(const tor_addr_t *low, const tor_addr_t *high,
                const char *country)
{
  intptr_t idx;
  void *idxplus1_;

  if (tor_addr_family(low) != tor_addr_family(high))
    return;
  if (tor_addr_compare(high, low, CMP_EXACT) < 0)
    return;

  idxplus1_ = strmap_get_lc(country_idxplus1_by_lc_code, country);

  if (!idxplus1_) {
    geoip_country_t *c = tor_malloc_zero(sizeof(geoip_country_t));
    strlcpy(c->countrycode, country, sizeof(c->countrycode));
    tor_strlower(c->countrycode);
    smartlist_add(geoip_countries, c);
    idx = smartlist_len(geoip_countries) - 1;
    strmap_set_lc(country_idxplus1_by_lc_code, country, (void*)(idx+1));
  } else {
    idx = ((uintptr_t)idxplus1_)-1;
  }
  {
    geoip_country_t *c = smartlist_get(geoip_countries, idx);
    tor_assert(!strcasecmp(c->countrycode, country));
  }

  if (tor_addr_family(low) == AF_INET) {
    geoip_ipv4_entry_t *ent = tor_malloc_zero(sizeof(geoip_ipv4_entry_t));
    ent->ip_low = tor_addr_to_ipv4h(low);
    ent->ip_high = tor_addr_to_ipv4h(high);
    ent->country = idx;
    smartlist_add(geoip_ipv4_entries, ent);
  } else if (tor_addr_family(low) == AF_INET6) {
    geoip_ipv6_entry_t *ent = tor_malloc_zero(sizeof(geoip_ipv6_entry_t));
    ent->ip_low = *tor_addr_to_in6(low);
    ent->ip_high = *tor_addr_to_in6(high);
    ent->country = idx;
    smartlist_add(geoip_ipv6_entries, ent);
  }
}

/** Add an entry to the GeoIP table indicated by <b>family</b>,
 * parsing it from <b>line</b>. The format is as for geoip_load_file(). */
STATIC int
geoip_parse_entry(const char *line, sa_family_t family)
{
  tor_addr_t low_addr, high_addr;
  char c[3];
  char *country = NULL;

  if (!geoip_countries)
    init_geoip_countries();
  if (family == AF_INET) {
    if (!geoip_ipv4_entries)
      geoip_ipv4_entries = smartlist_new();
  } else if (family == AF_INET6) {
    if (!geoip_ipv6_entries)
      geoip_ipv6_entries = smartlist_new();
  } else {
    log_warn(LD_GENERAL, "Unsupported family: %d", family);
    return -1;
  }

  while (TOR_ISSPACE(*line))
    ++line;
  if (*line == '#')
    return 0;

  if (family == AF_INET) {
    unsigned int low, high;
    if (tor_sscanf(line,"%u,%u,%2s", &low, &high, c) == 3 ||
        tor_sscanf(line,"\"%u\",\"%u\",\"%2s\",", &low, &high, c) == 3) {
      tor_addr_from_ipv4h(&low_addr, low);
      tor_addr_from_ipv4h(&high_addr, high);
    } else
      goto fail;
    country = c;
  } else {                      /* AF_INET6 */
    char buf[512];
    char *low_str, *high_str;
    struct in6_addr low, high;
    char *strtok_state;
    strlcpy(buf, line, sizeof(buf));
    low_str = tor_strtok_r(buf, ",", &strtok_state);
    if (!low_str)
      goto fail;
    high_str = tor_strtok_r(NULL, ",", &strtok_state);
    if (!high_str)
      goto fail;
    country = tor_strtok_r(NULL, "\n", &strtok_state);
    if (!country)
      goto fail;
    if (strlen(country) != 2)
      goto fail;
    if (tor_inet_pton(AF_INET6, low_str, &low) <= 0)
      goto fail;
    tor_addr_from_in6(&low_addr, &low);
    if (tor_inet_pton(AF_INET6, high_str, &high) <= 0)
      goto fail;
    tor_addr_from_in6(&high_addr, &high);
  }
  geoip_add_entry(&low_addr, &high_addr, country);
  return 0;

  fail:
  log_warn(LD_GENERAL, "Unable to parse line from GEOIP %s file: %s",
           family == AF_INET ? "IPv4" : "IPv6", escaped(line));
  return -1;
}

/** Sorting helper: return -1, 1, or 0 based on comparison of two
 * geoip_ipv4_entry_t */
static int
geoip_ipv4_compare_entries_(const void **_a, const void **_b)
{
  const geoip_ipv4_entry_t *a = *_a, *b = *_b;
  if (a->ip_low < b->ip_low)
    return -1;
  else if (a->ip_low > b->ip_low)
    return 1;
  else
    return 0;
}

/** bsearch helper: return -1, 1, or 0 based on comparison of an IP (a pointer
 * to a uint32_t in host order) to a geoip_ipv4_entry_t */
static int
geoip_ipv4_compare_key_to_entry_(const void *_key, const void **_member)
{
  /* No alignment issue here, since _key really is a pointer to uint32_t */
  const uint32_t addr = *(uint32_t *)_key;
  const geoip_ipv4_entry_t *entry = *_member;
  if (addr < entry->ip_low)
    return -1;
  else if (addr > entry->ip_high)
    return 1;
  else
    return 0;
}

/** Sorting helper: return -1, 1, or 0 based on comparison of two
 * geoip_ipv6_entry_t */
static int
geoip_ipv6_compare_entries_(const void **_a, const void **_b)
{
  const geoip_ipv6_entry_t *a = *_a, *b = *_b;
  return fast_memcmp(a->ip_low.s6_addr, b->ip_low.s6_addr,
                     sizeof(struct in6_addr));
}

/** bsearch helper: return -1, 1, or 0 based on comparison of an IPv6
 * (a pointer to a in6_addr) to a geoip_ipv6_entry_t */
static int
geoip_ipv6_compare_key_to_entry_(const void *_key, const void **_member)
{
  const struct in6_addr *addr = (struct in6_addr *)_key;
  const geoip_ipv6_entry_t *entry = *_member;

  if (fast_memcmp(addr->s6_addr, entry->ip_low.s6_addr,
             sizeof(struct in6_addr)) < 0)
    return -1;
  else if (fast_memcmp(addr->s6_addr, entry->ip_high.s6_addr,
                  sizeof(struct in6_addr)) > 0)
    return 1;
  else
    return 0;
}

/** Return 1 if we should collect geoip stats on bridge users, and
 * include them in our extrainfo descriptor. Else return 0. */
int
should_record_bridge_info(const or_options_t *options)
{
  return options->BridgeRelay && options->BridgeRecordUsageByCountry;
}

/** Set up a new list of geoip countries with no countries (yet) set in it,
 * except for the unknown country.
 */
static void
init_geoip_countries(void)
{
  geoip_country_t *geoip_unresolved;
  geoip_countries = smartlist_new();
  /* Add a geoip_country_t for requests that could not be resolved to a
   * country as first element (index 0) to geoip_countries. */
  geoip_unresolved = tor_malloc_zero(sizeof(geoip_country_t));
  strlcpy(geoip_unresolved->countrycode, "??",
          sizeof(geoip_unresolved->countrycode));
  smartlist_add(geoip_countries, geoip_unresolved);
  country_idxplus1_by_lc_code = strmap_new();
  strmap_set_lc(country_idxplus1_by_lc_code, "??", (void*)(1));
}

/** Clear appropriate GeoIP database, based on <b>family</b>, and
 * reload it from the file <b>filename</b>. Return 0 on success, -1 on
 * failure.
 *
 * Recognized line formats for IPv4 are:
 *   INTIPLOW,INTIPHIGH,CC
 * and
 *   "INTIPLOW","INTIPHIGH","CC","CC3","COUNTRY NAME"
 * where INTIPLOW and INTIPHIGH are IPv4 addresses encoded as 4-byte unsigned
 * integers, and CC is a country code.
 *
 * Recognized line format for IPv6 is:
 *   IPV6LOW,IPV6HIGH,CC
 * where IPV6LOW and IPV6HIGH are IPv6 addresses and CC is a country code.
 *
 * It also recognizes, and skips over, blank lines and lines that start
 * with '#' (comments).
 */
int
geoip_load_file(sa_family_t family, const char *filename)
{
  FILE *f;
  const char *msg = "";
  const or_options_t *options = get_options();
  int severity = options_need_geoip_info(options, &msg) ? LOG_WARN : LOG_INFO;
  crypto_digest_t *geoip_digest_env = NULL;

  tor_assert(family == AF_INET || family == AF_INET6);

  if (!(f = tor_fopen_cloexec(filename, "r"))) {
    log_fn(severity, LD_GENERAL, "Failed to open GEOIP file %s.  %s",
           filename, msg);
    return -1;
  }
  if (!geoip_countries)
    init_geoip_countries();

  if (family == AF_INET) {
    if (geoip_ipv4_entries) {
      SMARTLIST_FOREACH(geoip_ipv4_entries, geoip_ipv4_entry_t *, e,
                        tor_free(e));
      smartlist_free(geoip_ipv4_entries);
    }
    geoip_ipv4_entries = smartlist_new();
  } else { /* AF_INET6 */
    if (geoip_ipv6_entries) {
      SMARTLIST_FOREACH(geoip_ipv6_entries, geoip_ipv6_entry_t *, e,
                        tor_free(e));
      smartlist_free(geoip_ipv6_entries);
    }
    geoip_ipv6_entries = smartlist_new();
  }
  geoip_digest_env = crypto_digest_new();

  log_notice(LD_GENERAL, "Parsing GEOIP %s file %s.",
             (family == AF_INET) ? "IPv4" : "IPv6", filename);
  while (!feof(f)) {
    char buf[512];
    if (fgets(buf, (int)sizeof(buf), f) == NULL)
      break;
    crypto_digest_add_bytes(geoip_digest_env, buf, strlen(buf));
    /* FFFF track full country name. */
    geoip_parse_entry(buf, family);
  }
  /*XXXX abort and return -1 if no entries/illformed?*/
  fclose(f);

  /* Sort list and remember file digests so that we can include it in
   * our extra-info descriptors. */
  if (family == AF_INET) {
    smartlist_sort(geoip_ipv4_entries, geoip_ipv4_compare_entries_);
    /* Okay, now we need to maybe change our mind about what is in
     * which country. We do this for IPv4 only since that's what we
     * store in node->country. */
    refresh_all_country_info();
    crypto_digest_get_digest(geoip_digest_env, geoip_digest, DIGEST_LEN);
  } else {
    /* AF_INET6 */
    smartlist_sort(geoip_ipv6_entries, geoip_ipv6_compare_entries_);
    crypto_digest_get_digest(geoip_digest_env, geoip6_digest, DIGEST_LEN);
  }
  crypto_digest_free(geoip_digest_env);

  return 0;
}

/** Given an IP address in host order, return a number representing the
 * country to which that address belongs, -1 for "No geoip information
 * available", or 0 for the 'unknown country'.  The return value will always
 * be less than geoip_get_n_countries().  To decode it, call
 * geoip_get_country_name().
 */
STATIC int
geoip_get_country_by_ipv4(uint32_t ipaddr)
{
  geoip_ipv4_entry_t *ent;
  if (!geoip_ipv4_entries)
    return -1;
  ent = smartlist_bsearch(geoip_ipv4_entries, &ipaddr,
                          geoip_ipv4_compare_key_to_entry_);
  return ent ? (int)ent->country : 0;
}

/** Given an IPv6 address, return a number representing the country to
 * which that address belongs, -1 for "No geoip information available", or
 * 0 for the 'unknown country'.  The return value will always be less than
 * geoip_get_n_countries().  To decode it, call geoip_get_country_name().
 */
STATIC int
geoip_get_country_by_ipv6(const struct in6_addr *addr)
{
  geoip_ipv6_entry_t *ent;

  if (!geoip_ipv6_entries)
    return -1;
  ent = smartlist_bsearch(geoip_ipv6_entries, addr,
                          geoip_ipv6_compare_key_to_entry_);
  return ent ? (int)ent->country : 0;
}

/** Given an IP address, return a number representing the country to which
 * that address belongs, -1 for "No geoip information available", or 0 for
 * the 'unknown country'.  The return value will always be less than
 * geoip_get_n_countries().  To decode it, call geoip_get_country_name().
 */
int
geoip_get_country_by_addr(const tor_addr_t *addr)
{
  if (tor_addr_family(addr) == AF_INET) {
    return geoip_get_country_by_ipv4(tor_addr_to_ipv4h(addr));
  } else if (tor_addr_family(addr) == AF_INET6) {
    return geoip_get_country_by_ipv6(tor_addr_to_in6(addr));
  } else {
    return -1;
  }
}

/** Return the number of countries recognized by the GeoIP country list. */
int
geoip_get_n_countries(void)
{
  if (!geoip_countries)
    init_geoip_countries();
  return (int) smartlist_len(geoip_countries);
}

/** Return the two-letter country code associated with the number <b>num</b>,
 * or "??" for an unknown value. */
const char *
geoip_get_country_name(country_t num)
{
  if (geoip_countries && num >= 0 && num < smartlist_len(geoip_countries)) {
    geoip_country_t *c = smartlist_get(geoip_countries, num);
    return c->countrycode;
  } else
    return "??";
}

/** Return true iff we have loaded a GeoIP database.*/
int
geoip_is_loaded(sa_family_t family)
{
  tor_assert(family == AF_INET || family == AF_INET6);
  if (geoip_countries == NULL)
    return 0;
  if (family == AF_INET)
    return geoip_ipv4_entries != NULL;
  else                          /* AF_INET6 */
    return geoip_ipv6_entries != NULL;
}

/** Return the hex-encoded SHA1 digest of the loaded GeoIP file. The
 * result does not need to be deallocated, but will be overwritten by the
 * next call of hex_str(). */
const char *
geoip_db_digest(sa_family_t family)
{
  tor_assert(family == AF_INET || family == AF_INET6);
  if (family == AF_INET)
    return hex_str(geoip_digest, DIGEST_LEN);
  else                          /* AF_INET6 */
    return hex_str(geoip6_digest, DIGEST_LEN);
}

/** Entry in a map from IP address to the last time we've seen an incoming
 * connection from that IP address. Used by bridges only, to track which
 * countries have them blocked. */
typedef struct clientmap_entry_t {
  HT_ENTRY(clientmap_entry_t) node;
  tor_addr_t addr;
 /* Name of pluggable transport used by this client. NULL if no
    pluggable transport was used. */
  char *transport_name;

  /** Time when we last saw this IP address, in MINUTES since the epoch.
   *
   * (This will run out of space around 4011 CE.  If Tor is still in use around
   * 4000 CE, please remember to add more bits to last_seen_in_minutes.) */
  unsigned int last_seen_in_minutes:30;
  unsigned int action:2;
} clientmap_entry_t;

/** Largest allowable value for last_seen_in_minutes.  (It's a 30-bit field,
 * so it can hold up to (1u<<30)-1, or 0x3fffffffu.
 */
#define MAX_LAST_SEEN_IN_MINUTES 0X3FFFFFFFu

/** Map from client IP address to last time seen. */
static HT_HEAD(clientmap, clientmap_entry_t) client_history =
     HT_INITIALIZER();

/** Hashtable helper: compute a hash of a clientmap_entry_t. */
static INLINE unsigned
clientmap_entry_hash(const clientmap_entry_t *a)
{
  unsigned h = (unsigned) tor_addr_hash(&a->addr);

  if (a->transport_name)
    h += (unsigned) siphash24g(a->transport_name, strlen(a->transport_name));

  return h;
}
/** Hashtable helper: compare two clientmap_entry_t values for equality. */
static INLINE int
clientmap_entries_eq(const clientmap_entry_t *a, const clientmap_entry_t *b)
{
  if (strcmp_opt(a->transport_name, b->transport_name))
    return 0;

  return !tor_addr_compare(&a->addr, &b->addr, CMP_EXACT) &&
         a->action == b->action;
}

HT_PROTOTYPE(clientmap, clientmap_entry_t, node, clientmap_entry_hash,
             clientmap_entries_eq);
HT_GENERATE(clientmap, clientmap_entry_t, node, clientmap_entry_hash,
            clientmap_entries_eq, 0.6, malloc, realloc, free);

/** Free all storage held by <b>ent</b>. */
static void
clientmap_entry_free(clientmap_entry_t *ent)
{
  if (!ent)
    return;

  tor_free(ent->transport_name);
  tor_free(ent);
}

/** Clear history of connecting clients used by entry and bridge stats. */
static void
client_history_clear(void)
{
  clientmap_entry_t **ent, **next, *this;
  for (ent = HT_START(clientmap, &client_history); ent != NULL;
       ent = next) {
    if ((*ent)->action == GEOIP_CLIENT_CONNECT) {
      this = *ent;
      next = HT_NEXT_RMV(clientmap, &client_history, ent);
      clientmap_entry_free(this);
    } else {
      next = HT_NEXT(clientmap, &client_history, ent);
    }
  }
}

/** Note that we've seen a client connect from the IP <b>addr</b>
 * at time <b>now</b>. Ignored by all but bridges and directories if
 * configured accordingly. */
void
geoip_note_client_seen(geoip_client_action_t action,
                       const tor_addr_t *addr,
                       const char *transport_name,
                       time_t now)
{
  const or_options_t *options = get_options();
  clientmap_entry_t lookup, *ent;
  memset(&lookup, 0, sizeof(clientmap_entry_t));

  if (action == GEOIP_CLIENT_CONNECT) {
    /* Only remember statistics as entry guard or as bridge. */
    if (!options->EntryStatistics &&
        (!(options->BridgeRelay && options->BridgeRecordUsageByCountry)))
      return;
  } else {
    /* Only gather directory-request statistics if configured, and
     * forcibly disable them on bridge authorities. */
    if (!options->DirReqStatistics || options->BridgeAuthoritativeDir)
      return;
  }

  log_debug(LD_GENERAL, "Seen client from '%s' with transport '%s'.",
            safe_str_client(fmt_addr((addr))),
            transport_name ? transport_name : "<no transport>");

  tor_addr_copy(&lookup.addr, addr);
  lookup.action = (int)action;
  lookup.transport_name = (char*) transport_name;
  ent = HT_FIND(clientmap, &client_history, &lookup);

  if (! ent) {
    ent = tor_malloc_zero(sizeof(clientmap_entry_t));
    tor_addr_copy(&ent->addr, addr);
    if (transport_name)
      ent->transport_name = tor_strdup(transport_name);
    ent->action = (int)action;
    HT_INSERT(clientmap, &client_history, ent);
  }
  if (now / 60 <= (int)MAX_LAST_SEEN_IN_MINUTES && now >= 0)
    ent->last_seen_in_minutes = (unsigned)(now/60);
  else
    ent->last_seen_in_minutes = 0;

  if (action == GEOIP_CLIENT_NETWORKSTATUS) {
    int country_idx = geoip_get_country_by_addr(addr);
    if (country_idx < 0)
      country_idx = 0; /** unresolved requests are stored at index 0. */
    if (country_idx >= 0 && country_idx < smartlist_len(geoip_countries)) {
      geoip_country_t *country = smartlist_get(geoip_countries, country_idx);
      ++country->n_v3_ns_requests;
    }
  }
}

/** HT_FOREACH helper: remove a clientmap_entry_t from the hashtable if it's
 * older than a certain time. */
static int
remove_old_client_helper_(struct clientmap_entry_t *ent, void *_cutoff)
{
  time_t cutoff = *(time_t*)_cutoff / 60;
  if (ent->last_seen_in_minutes < cutoff) {
    clientmap_entry_free(ent);
    return 1;
  } else {
    return 0;
  }
}

/** Forget about all clients that haven't connected since <b>cutoff</b>. */
void
geoip_remove_old_clients(time_t cutoff)
{
  clientmap_HT_FOREACH_FN(&client_history,
                          remove_old_client_helper_,
                          &cutoff);
}

/** How many responses are we giving to clients requesting v3 network
 * statuses? */
static uint32_t ns_v3_responses[GEOIP_NS_RESPONSE_NUM];

/** Note that we've rejected a client's request for a v3 network status
 * for reason <b>reason</b> at time <b>now</b>. */
void
geoip_note_ns_response(geoip_ns_response_t response)
{
  static int arrays_initialized = 0;
  if (!get_options()->DirReqStatistics)
    return;
  if (!arrays_initialized) {
    memset(ns_v3_responses, 0, sizeof(ns_v3_responses));
    arrays_initialized = 1;
  }
  tor_assert(response < GEOIP_NS_RESPONSE_NUM);
  ns_v3_responses[response]++;
}

/** Do not mention any country from which fewer than this number of IPs have
 * connected.  This conceivably avoids reporting information that could
 * deanonymize users, though analysis is lacking. */
#define MIN_IPS_TO_NOTE_COUNTRY 1
/** Do not report any geoip data at all if we have fewer than this number of
 * IPs to report about. */
#define MIN_IPS_TO_NOTE_ANYTHING 1
/** When reporting geoip data about countries, round up to the nearest
 * multiple of this value. */
#define IP_GRANULARITY 8

/** Helper type: used to sort per-country totals by value. */
typedef struct c_hist_t {
  char country[3]; /**< Two-letter country code. */
  unsigned total; /**< Total IP addresses seen in this country. */
} c_hist_t;

/** Sorting helper: return -1, 1, or 0 based on comparison of two
 * geoip_ipv4_entry_t.  Sort in descending order of total, and then by country
 * code. */
static int
c_hist_compare_(const void **_a, const void **_b)
{
  const c_hist_t *a = *_a, *b = *_b;
  if (a->total > b->total)
    return -1;
  else if (a->total < b->total)
    return 1;
  else
    return strcmp(a->country, b->country);
}

/** When there are incomplete directory requests at the end of a 24-hour
 * period, consider those requests running for longer than this timeout as
 * failed, the others as still running. */
#define DIRREQ_TIMEOUT (10*60)

/** Entry in a map from either chan->global_identifier for direct requests
 * or a unique circuit identifier for tunneled requests to request time,
 * response size, and completion time of a network status request. Used to
 * measure download times of requests to derive average client
 * bandwidths. */
typedef struct dirreq_map_entry_t {
  HT_ENTRY(dirreq_map_entry_t) node;
  /** Unique identifier for this network status request; this is either the
   * chan->global_identifier of the dir channel (direct request) or a new
   * locally unique identifier of a circuit (tunneled request). This ID is
   * only unique among other direct or tunneled requests, respectively. */
  uint64_t dirreq_id;
  unsigned int state:3; /**< State of this directory request. */
  unsigned int type:1; /**< Is this a direct or a tunneled request? */
  unsigned int completed:1; /**< Is this request complete? */
  /** When did we receive the request and started sending the response? */
  struct timeval request_time;
  size_t response_size; /**< What is the size of the response in bytes? */
  struct timeval completion_time; /**< When did the request succeed? */
} dirreq_map_entry_t;

/** Map of all directory requests asking for v2 or v3 network statuses in
 * the current geoip-stats interval. Values are
 * of type *<b>dirreq_map_entry_t</b>. */
static HT_HEAD(dirreqmap, dirreq_map_entry_t) dirreq_map =
     HT_INITIALIZER();

static int
dirreq_map_ent_eq(const dirreq_map_entry_t *a,
                  const dirreq_map_entry_t *b)
{
  return a->dirreq_id == b->dirreq_id && a->type == b->type;
}

/* DOCDOC dirreq_map_ent_hash */
static unsigned
dirreq_map_ent_hash(const dirreq_map_entry_t *entry)
{
  unsigned u = (unsigned) entry->dirreq_id;
  u += entry->type << 20;
  return u;
}

HT_PROTOTYPE(dirreqmap, dirreq_map_entry_t, node, dirreq_map_ent_hash,
             dirreq_map_ent_eq);
HT_GENERATE(dirreqmap, dirreq_map_entry_t, node, dirreq_map_ent_hash,
            dirreq_map_ent_eq, 0.6, malloc, realloc, free);

/** Helper: Put <b>entry</b> into map of directory requests using
 * <b>type</b> and <b>dirreq_id</b> as key parts. If there is
 * already an entry for that key, print out a BUG warning and return. */
static void
dirreq_map_put_(dirreq_map_entry_t *entry, dirreq_type_t type,
               uint64_t dirreq_id)
{
  dirreq_map_entry_t *old_ent;
  tor_assert(entry->type == type);
  tor_assert(entry->dirreq_id == dirreq_id);

  /* XXXX we could switch this to HT_INSERT some time, since it seems that
   * this bug doesn't happen. But since this function doesn't seem to be
   * critical-path, it's sane to leave it alone. */
  old_ent = HT_REPLACE(dirreqmap, &dirreq_map, entry);
  if (old_ent && old_ent != entry) {
    log_warn(LD_BUG, "Error when putting directory request into local "
             "map. There was already an entry for the same identifier.");
    return;
  }
}

/** Helper: Look up and return an entry in the map of directory requests
 * using <b>type</b> and <b>dirreq_id</b> as key parts. If there
 * is no such entry, return NULL. */
static dirreq_map_entry_t *
dirreq_map_get_(dirreq_type_t type, uint64_t dirreq_id)
{
  dirreq_map_entry_t lookup;
  lookup.type = type;
  lookup.dirreq_id = dirreq_id;
  return HT_FIND(dirreqmap, &dirreq_map, &lookup);
}

/** Note that an either direct or tunneled (see <b>type</b>) directory
 * request for a v3 network status with unique ID <b>dirreq_id</b> of size
 * <b>response_size</b> has started. */
void
geoip_start_dirreq(uint64_t dirreq_id, size_t response_size,
                   dirreq_type_t type)
{
  dirreq_map_entry_t *ent;
  if (!get_options()->DirReqStatistics)
    return;
  ent = tor_malloc_zero(sizeof(dirreq_map_entry_t));
  ent->dirreq_id = dirreq_id;
  tor_gettimeofday(&ent->request_time);
  ent->response_size = response_size;
  ent->type = type;
  dirreq_map_put_(ent, type, dirreq_id);
}

/** Change the state of the either direct or tunneled (see <b>type</b>)
 * directory request with <b>dirreq_id</b> to <b>new_state</b> and
 * possibly mark it as completed. If no entry can be found for the given
 * key parts (e.g., if this is a directory request that we are not
 * measuring, or one that was started in the previous measurement period),
 * or if the state cannot be advanced to <b>new_state</b>, do nothing. */
void
geoip_change_dirreq_state(uint64_t dirreq_id, dirreq_type_t type,
                          dirreq_state_t new_state)
{
  dirreq_map_entry_t *ent;
  if (!get_options()->DirReqStatistics)
    return;
  ent = dirreq_map_get_(type, dirreq_id);
  if (!ent)
    return;
  if (new_state == DIRREQ_IS_FOR_NETWORK_STATUS)
    return;
  if (new_state - 1 != ent->state)
    return;
  ent->state = new_state;
  if ((type == DIRREQ_DIRECT &&
         new_state == DIRREQ_FLUSHING_DIR_CONN_FINISHED) ||
      (type == DIRREQ_TUNNELED &&
         new_state == DIRREQ_CHANNEL_BUFFER_FLUSHED)) {
    tor_gettimeofday(&ent->completion_time);
    ent->completed = 1;
  }
}

/** Return the bridge-ip-transports string that should be inserted in
 *  our extra-info descriptor. Return NULL if the bridge-ip-transports
 *  line should be empty.  */
char *
geoip_get_transport_history(void)
{
  unsigned granularity = IP_GRANULARITY;
  /** String hash table (name of transport) -> (number of users). */
  strmap_t *transport_counts = strmap_new();

  /** Smartlist that contains copies of the names of the transports
      that have been used. */
  smartlist_t *transports_used = smartlist_new();

  /* Special string to signify that no transport was used for this
     connection. Pluggable transport names can't have symbols in their
     names, so this string will never collide with a real transport. */
  static const char* no_transport_str = "<OR>";

  clientmap_entry_t **ent;
  const char *transport_name = NULL;
  smartlist_t *string_chunks = smartlist_new();
  char *the_string = NULL;

  /* If we haven't seen any clients yet, return NULL. */
  if (HT_EMPTY(&client_history))
    goto done;

  /** We do the following steps to form the transport history string:
   *  a) Foreach client that uses a pluggable transport, we increase the
   *  times that transport was used by one. If the client did not use
   *  a transport, we increase the number of times someone connected
   *  without obfuscation.
   *  b) Foreach transport we observed, we write its transport history
   *  string and push it to string_chunks. So, for example, if we've
   *  seen 665 obfs2 clients, we write "obfs2=665".
   *  c) We concatenate string_chunks to form the final string.
   */

  log_debug(LD_GENERAL,"Starting iteration for transport history. %d clients.",
            HT_SIZE(&client_history));

  /* Loop through all clients. */
  HT_FOREACH(ent, clientmap, &client_history) {
    uintptr_t val;
    void *ptr;
    transport_name = (*ent)->transport_name;
    if (!transport_name)
      transport_name = no_transport_str;

    /* Increase the count for this transport name. */
    ptr = strmap_get(transport_counts, transport_name);
    val = (uintptr_t)ptr;
    val++;
    ptr = (void*)val;
    strmap_set(transport_counts, transport_name, ptr);

    /* If it's the first time we see this transport, note it. */
    if (val == 1)
      smartlist_add(transports_used, tor_strdup(transport_name));

    log_debug(LD_GENERAL, "Client from '%s' with transport '%s'. "
              "I've now seen %d clients.",
              safe_str_client(fmt_addr(&(*ent)->addr)),
              transport_name ? transport_name : "<no transport>",
              (int)val);
  }

  /* Sort the transport names (helps with unit testing). */
  smartlist_sort_strings(transports_used);

  /* Loop through all seen transports. */
  SMARTLIST_FOREACH_BEGIN(transports_used, const char *, transport_name) {
    void *transport_count_ptr = strmap_get(transport_counts, transport_name);
    uintptr_t transport_count = (uintptr_t) transport_count_ptr;

    log_debug(LD_GENERAL, "We got "U64_FORMAT" clients with transport '%s'.",
              U64_PRINTF_ARG((uint64_t)transport_count), transport_name);

    smartlist_add_asprintf(string_chunks, "%s="U64_FORMAT,
                           transport_name,
                           U64_PRINTF_ARG(round_uint64_to_next_multiple_of(
                                               (uint64_t)transport_count,
                                               granularity)));
  } SMARTLIST_FOREACH_END(transport_name);

  the_string = smartlist_join_strings(string_chunks, ",", 0, NULL);

  log_debug(LD_GENERAL, "Final bridge-ip-transports string: '%s'", the_string);

 done:
  strmap_free(transport_counts, NULL);
  SMARTLIST_FOREACH(transports_used, char *, s, tor_free(s));
  smartlist_free(transports_used);
  SMARTLIST_FOREACH(string_chunks, char *, s, tor_free(s));
  smartlist_free(string_chunks);

  return the_string;
}

/** Return a newly allocated comma-separated string containing statistics
 * on network status downloads. The string contains the number of completed
 * requests, timeouts, and still running requests as well as the download
 * times by deciles and quartiles. Return NULL if we have not observed
 * requests for long enough. */
static char *
geoip_get_dirreq_history(dirreq_type_t type)
{
  char *result = NULL;
  smartlist_t *dirreq_completed = NULL;
  uint32_t complete = 0, timeouts = 0, running = 0;
  int bufsize = 1024, written;
  dirreq_map_entry_t **ptr, **next, *ent;
  struct timeval now;

  tor_gettimeofday(&now);
  dirreq_completed = smartlist_new();
  for (ptr = HT_START(dirreqmap, &dirreq_map); ptr; ptr = next) {
    ent = *ptr;
    if (ent->type != type) {
      next = HT_NEXT(dirreqmap, &dirreq_map, ptr);
      continue;
    } else {
      if (ent->completed) {
        smartlist_add(dirreq_completed, ent);
        complete++;
        next = HT_NEXT_RMV(dirreqmap, &dirreq_map, ptr);
      } else {
        if (tv_mdiff(&ent->request_time, &now) / 1000 > DIRREQ_TIMEOUT)
          timeouts++;
        else
          running++;
        next = HT_NEXT_RMV(dirreqmap, &dirreq_map, ptr);
        tor_free(ent);
      }
    }
  }
#define DIR_REQ_GRANULARITY 4
  complete = round_uint32_to_next_multiple_of(complete,
                                              DIR_REQ_GRANULARITY);
  timeouts = round_uint32_to_next_multiple_of(timeouts,
                                              DIR_REQ_GRANULARITY);
  running = round_uint32_to_next_multiple_of(running,
                                             DIR_REQ_GRANULARITY);
  result = tor_malloc_zero(bufsize);
  written = tor_snprintf(result, bufsize, "complete=%u,timeout=%u,"
                         "running=%u", complete, timeouts, running);
  if (written < 0) {
    tor_free(result);
    goto done;
  }

#define MIN_DIR_REQ_RESPONSES 16
  if (complete >= MIN_DIR_REQ_RESPONSES) {
    uint32_t *dltimes;
    /* We may have rounded 'completed' up.  Here we want to use the
     * real value. */
    complete = smartlist_len(dirreq_completed);
    dltimes = tor_malloc_zero(sizeof(uint32_t) * complete);
    SMARTLIST_FOREACH_BEGIN(dirreq_completed, dirreq_map_entry_t *, ent) {
      uint32_t bytes_per_second;
      uint32_t time_diff = (uint32_t) tv_mdiff(&ent->request_time,
                                               &ent->completion_time);
      if (time_diff == 0)
        time_diff = 1; /* Avoid DIV/0; "instant" answers are impossible
                        * by law of nature or something, but a milisecond
                        * is a bit greater than "instantly" */
      bytes_per_second = (uint32_t)(1000 * ent->response_size / time_diff);
      dltimes[ent_sl_idx] = bytes_per_second;
    } SMARTLIST_FOREACH_END(ent);
    median_uint32(dltimes, complete); /* sorts as a side effect. */
    written = tor_snprintf(result + written, bufsize - written,
                           ",min=%u,d1=%u,d2=%u,q1=%u,d3=%u,d4=%u,md=%u,"
                           "d6=%u,d7=%u,q3=%u,d8=%u,d9=%u,max=%u",
                           dltimes[0],
                           dltimes[1*complete/10-1],
                           dltimes[2*complete/10-1],
                           dltimes[1*complete/4-1],
                           dltimes[3*complete/10-1],
                           dltimes[4*complete/10-1],
                           dltimes[5*complete/10-1],
                           dltimes[6*complete/10-1],
                           dltimes[7*complete/10-1],
                           dltimes[3*complete/4-1],
                           dltimes[8*complete/10-1],
                           dltimes[9*complete/10-1],
                           dltimes[complete-1]);
    if (written<0)
      tor_free(result);
    tor_free(dltimes);
  }
 done:
  SMARTLIST_FOREACH(dirreq_completed, dirreq_map_entry_t *, ent,
                    tor_free(ent));
  smartlist_free(dirreq_completed);
  return result;
}

/** Store a newly allocated comma-separated string in
 * *<a>country_str</a> containing entries for all the countries from
 * which we've seen enough clients connect as a bridge, directory
 * server, or entry guard. The entry format is cc=num where num is the
 * number of IPs we've seen connecting from that country, and cc is a
 * lowercased country code. *<a>country_str</a> is set to NULL if
 * we're not ready to export per country data yet.
 *
 * Store a newly allocated comma-separated string in <a>ipver_str</a>
 * containing entries for clients connecting over IPv4 and IPv6. The
 * format is family=num where num is the nubmer of IPs we've seen
 * connecting over that protocol family, and family is 'v4' or 'v6'.
 *
 * Return 0 on success and -1 if we're missing geoip data. */
int
geoip_get_client_history(geoip_client_action_t action,
                         char **country_str, char **ipver_str)
{
  unsigned granularity = IP_GRANULARITY;
  smartlist_t *entries = NULL;
  int n_countries = geoip_get_n_countries();
  int i;
  clientmap_entry_t **ent;
  unsigned *counts = NULL;
  unsigned total = 0;
  unsigned ipv4_count = 0, ipv6_count = 0;

  if (!geoip_is_loaded(AF_INET) && !geoip_is_loaded(AF_INET6))
    return -1;

  counts = tor_malloc_zero(sizeof(unsigned)*n_countries);
  HT_FOREACH(ent, clientmap, &client_history) {
    int country;
    if ((*ent)->action != (int)action)
      continue;
    country = geoip_get_country_by_addr(&(*ent)->addr);
    if (country < 0)
      country = 0; /** unresolved requests are stored at index 0. */
    tor_assert(0 <= country && country < n_countries);
    ++counts[country];
    ++total;
    switch (tor_addr_family(&(*ent)->addr)) {
    case AF_INET:
      ipv4_count++;
      break;
    case AF_INET6:
      ipv6_count++;
      break;
    }
  }
  if (ipver_str) {
    smartlist_t *chunks = smartlist_new();
    smartlist_add_asprintf(chunks, "v4=%u",
                           round_to_next_multiple_of(ipv4_count, granularity));
    smartlist_add_asprintf(chunks, "v6=%u",
                           round_to_next_multiple_of(ipv6_count, granularity));
    *ipver_str = smartlist_join_strings(chunks, ",", 0, NULL);
    SMARTLIST_FOREACH(chunks, char *, c, tor_free(c));
    smartlist_free(chunks);
  }

  /* Don't record per country data if we haven't seen enough IPs. */
  if (total < MIN_IPS_TO_NOTE_ANYTHING) {
    tor_free(counts);
    if (country_str)
      *country_str = NULL;
    return 0;
  }

  /* Make a list of c_hist_t */
  entries = smartlist_new();
  for (i = 0; i < n_countries; ++i) {
    unsigned c = counts[i];
    const char *countrycode;
    c_hist_t *ent;
    /* Only report a country if it has a minimum number of IPs. */
    if (c >= MIN_IPS_TO_NOTE_COUNTRY) {
      c = round_to_next_multiple_of(c, granularity);
      countrycode = geoip_get_country_name(i);
      ent = tor_malloc(sizeof(c_hist_t));
      strlcpy(ent->country, countrycode, sizeof(ent->country));
      ent->total = c;
      smartlist_add(entries, ent);
    }
  }
  /* Sort entries. Note that we must do this _AFTER_ rounding, or else
   * the sort order could leak info. */
  smartlist_sort(entries, c_hist_compare_);

  if (country_str) {
    smartlist_t *chunks = smartlist_new();
    SMARTLIST_FOREACH(entries, c_hist_t *, ch, {
        smartlist_add_asprintf(chunks, "%s=%u", ch->country, ch->total);
      });
    *country_str = smartlist_join_strings(chunks, ",", 0, NULL);
    SMARTLIST_FOREACH(chunks, char *, c, tor_free(c));
    smartlist_free(chunks);
  }

  SMARTLIST_FOREACH(entries, c_hist_t *, c, tor_free(c));
  smartlist_free(entries);
  tor_free(counts);

  return 0;
}

/** Return a newly allocated string holding the per-country request history
 * for v3 network statuses in a format suitable for an extra-info document,
 * or NULL on failure. */
char *
geoip_get_request_history(void)
{
  smartlist_t *entries, *strings;
  char *result;
  unsigned granularity = IP_GRANULARITY;

  if (!geoip_countries)
    return NULL;

  entries = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(geoip_countries, geoip_country_t *, c) {
      uint32_t tot = 0;
      c_hist_t *ent;
      tot = c->n_v3_ns_requests;
      if (!tot)
        continue;
      ent = tor_malloc_zero(sizeof(c_hist_t));
      strlcpy(ent->country, c->countrycode, sizeof(ent->country));
      ent->total = round_to_next_multiple_of(tot, granularity);
      smartlist_add(entries, ent);
  } SMARTLIST_FOREACH_END(c);
  smartlist_sort(entries, c_hist_compare_);

  strings = smartlist_new();
  SMARTLIST_FOREACH(entries, c_hist_t *, ent, {
      smartlist_add_asprintf(strings, "%s=%u", ent->country, ent->total);
  });
  result = smartlist_join_strings(strings, ",", 0, NULL);
  SMARTLIST_FOREACH(strings, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(entries, c_hist_t *, ent, tor_free(ent));
  smartlist_free(strings);
  smartlist_free(entries);
  return result;
}

/** Start time of directory request stats or 0 if we're not collecting
 * directory request statistics. */
static time_t start_of_dirreq_stats_interval;

/** Initialize directory request stats. */
void
geoip_dirreq_stats_init(time_t now)
{
  start_of_dirreq_stats_interval = now;
}

/** Reset counters for dirreq stats. */
void
geoip_reset_dirreq_stats(time_t now)
{
  SMARTLIST_FOREACH(geoip_countries, geoip_country_t *, c, {
      c->n_v3_ns_requests = 0;
  });
  {
    clientmap_entry_t **ent, **next, *this;
    for (ent = HT_START(clientmap, &client_history); ent != NULL;
         ent = next) {
      if ((*ent)->action == GEOIP_CLIENT_NETWORKSTATUS) {
        this = *ent;
        next = HT_NEXT_RMV(clientmap, &client_history, ent);
        clientmap_entry_free(this);
      } else {
        next = HT_NEXT(clientmap, &client_history, ent);
      }
    }
  }
  memset(ns_v3_responses, 0, sizeof(ns_v3_responses));
  {
    dirreq_map_entry_t **ent, **next, *this;
    for (ent = HT_START(dirreqmap, &dirreq_map); ent != NULL; ent = next) {
      this = *ent;
      next = HT_NEXT_RMV(dirreqmap, &dirreq_map, ent);
      tor_free(this);
    }
  }
  start_of_dirreq_stats_interval = now;
}

/** Stop collecting directory request stats in a way that we can re-start
 * doing so in geoip_dirreq_stats_init(). */
void
geoip_dirreq_stats_term(void)
{
  geoip_reset_dirreq_stats(0);
}

/** Return a newly allocated string containing the dirreq statistics
 * until <b>now</b>, or NULL if we're not collecting dirreq stats. Caller
 * must ensure start_of_dirreq_stats_interval is in the past. */
char *
geoip_format_dirreq_stats(time_t now)
{
  char t[ISO_TIME_LEN+1];
  int i;
  char *v3_ips_string, *v3_reqs_string, *v3_direct_dl_string,
       *v3_tunneled_dl_string;
  char *result;

  if (!start_of_dirreq_stats_interval)
    return NULL; /* Not initialized. */

  tor_assert(now >= start_of_dirreq_stats_interval);

  format_iso_time(t, now);
  geoip_get_client_history(GEOIP_CLIENT_NETWORKSTATUS, &v3_ips_string, NULL);
  v3_reqs_string = geoip_get_request_history();

#define RESPONSE_GRANULARITY 8
  for (i = 0; i < GEOIP_NS_RESPONSE_NUM; i++) {
    ns_v3_responses[i] = round_uint32_to_next_multiple_of(
                               ns_v3_responses[i], RESPONSE_GRANULARITY);
  }
#undef RESPONSE_GRANULARITY

  v3_direct_dl_string = geoip_get_dirreq_history(DIRREQ_DIRECT);
  v3_tunneled_dl_string = geoip_get_dirreq_history(DIRREQ_TUNNELED);

  /* Put everything together into a single string. */
  tor_asprintf(&result, "dirreq-stats-end %s (%d s)\n"
              "dirreq-v3-ips %s\n"
              "dirreq-v3-reqs %s\n"
              "dirreq-v3-resp ok=%u,not-enough-sigs=%u,unavailable=%u,"
                   "not-found=%u,not-modified=%u,busy=%u\n"
              "dirreq-v3-direct-dl %s\n"
              "dirreq-v3-tunneled-dl %s\n",
              t,
              (unsigned) (now - start_of_dirreq_stats_interval),
              v3_ips_string ? v3_ips_string : "",
              v3_reqs_string ? v3_reqs_string : "",
              ns_v3_responses[GEOIP_SUCCESS],
              ns_v3_responses[GEOIP_REJECT_NOT_ENOUGH_SIGS],
              ns_v3_responses[GEOIP_REJECT_UNAVAILABLE],
              ns_v3_responses[GEOIP_REJECT_NOT_FOUND],
              ns_v3_responses[GEOIP_REJECT_NOT_MODIFIED],
              ns_v3_responses[GEOIP_REJECT_BUSY],
              v3_direct_dl_string ? v3_direct_dl_string : "",
              v3_tunneled_dl_string ? v3_tunneled_dl_string : "");

  /* Free partial strings. */
  tor_free(v3_ips_string);
  tor_free(v3_reqs_string);
  tor_free(v3_direct_dl_string);
  tor_free(v3_tunneled_dl_string);

  return result;
}

/** If 24 hours have passed since the beginning of the current dirreq
 * stats period, write dirreq stats to $DATADIR/stats/dirreq-stats
 * (possibly overwriting an existing file) and reset counters.  Return
 * when we would next want to write dirreq stats or 0 if we never want to
 * write. */
time_t
geoip_dirreq_stats_write(time_t now)
{
  char *str = NULL;

  if (!start_of_dirreq_stats_interval)
    return 0; /* Not initialized. */
  if (start_of_dirreq_stats_interval + WRITE_STATS_INTERVAL > now)
    goto done; /* Not ready to write. */

  /* Discard all items in the client history that are too old. */
  geoip_remove_old_clients(start_of_dirreq_stats_interval);

  /* Generate history string .*/
  str = geoip_format_dirreq_stats(now);

  /* Write dirreq-stats string to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "dirreq-stats", str, "dirreq statistics");
    /* Reset measurement interval start. */
    geoip_reset_dirreq_stats(now);
  }

 done:
  tor_free(str);
  return start_of_dirreq_stats_interval + WRITE_STATS_INTERVAL;
}

/** Start time of bridge stats or 0 if we're not collecting bridge
 * statistics. */
static time_t start_of_bridge_stats_interval;

/** Initialize bridge stats. */
void
geoip_bridge_stats_init(time_t now)
{
  start_of_bridge_stats_interval = now;
}

/** Stop collecting bridge stats in a way that we can re-start doing so in
 * geoip_bridge_stats_init(). */
void
geoip_bridge_stats_term(void)
{
  client_history_clear();
  start_of_bridge_stats_interval = 0;
}

/** Validate a bridge statistics string as it would be written to a
 * current extra-info descriptor. Return 1 if the string is valid and
 * recent enough, or 0 otherwise. */
static int
validate_bridge_stats(const char *stats_str, time_t now)
{
  char stats_end_str[ISO_TIME_LEN+1], stats_start_str[ISO_TIME_LEN+1],
       *eos;

  const char *BRIDGE_STATS_END = "bridge-stats-end ";
  const char *BRIDGE_IPS = "bridge-ips ";
  const char *BRIDGE_IPS_EMPTY_LINE = "bridge-ips\n";
  const char *BRIDGE_TRANSPORTS = "bridge-ip-transports ";
  const char *BRIDGE_TRANSPORTS_EMPTY_LINE = "bridge-ip-transports\n";
  const char *tmp;
  time_t stats_end_time;
  int seconds;
  tor_assert(stats_str);

  /* Parse timestamp and number of seconds from
    "bridge-stats-end YYYY-MM-DD HH:MM:SS (N s)" */
  tmp = find_str_at_start_of_line(stats_str, BRIDGE_STATS_END);
  if (!tmp)
    return 0;
  tmp += strlen(BRIDGE_STATS_END);

  if (strlen(tmp) < ISO_TIME_LEN + 6)
    return 0;
  strlcpy(stats_end_str, tmp, sizeof(stats_end_str));
  if (parse_iso_time(stats_end_str, &stats_end_time) < 0)
    return 0;
  if (stats_end_time < now - (25*60*60) ||
      stats_end_time > now + (1*60*60))
    return 0;
  seconds = (int)strtol(tmp + ISO_TIME_LEN + 2, &eos, 10);
  if (!eos || seconds < 23*60*60)
    return 0;
  format_iso_time(stats_start_str, stats_end_time - seconds);

  /* Parse: "bridge-ips CC=N,CC=N,..." */
  tmp = find_str_at_start_of_line(stats_str, BRIDGE_IPS);
  if (!tmp) {
    /* Look if there is an empty "bridge-ips" line */
    tmp = find_str_at_start_of_line(stats_str, BRIDGE_IPS_EMPTY_LINE);
    if (!tmp)
      return 0;
  }

  /* Parse: "bridge-ip-transports PT=N,PT=N,..." */
  tmp = find_str_at_start_of_line(stats_str, BRIDGE_TRANSPORTS);
  if (!tmp) {
    /* Look if there is an empty "bridge-ip-transports" line */
    tmp = find_str_at_start_of_line(stats_str, BRIDGE_TRANSPORTS_EMPTY_LINE);
    if (!tmp)
      return 0;
  }

  return 1;
}

/** Most recent bridge statistics formatted to be written to extra-info
 * descriptors. */
static char *bridge_stats_extrainfo = NULL;

/** Return a newly allocated string holding our bridge usage stats by country
 * in a format suitable for inclusion in an extrainfo document. Return NULL on
 * failure.  */
char *
geoip_format_bridge_stats(time_t now)
{
  char *out = NULL;
  char *country_data = NULL, *ipver_data = NULL, *transport_data = NULL;
  long duration = now - start_of_bridge_stats_interval;
  char written[ISO_TIME_LEN+1];

  if (duration < 0)
    return NULL;
  if (!start_of_bridge_stats_interval)
    return NULL; /* Not initialized. */

  format_iso_time(written, now);
  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &country_data, &ipver_data);
  transport_data = geoip_get_transport_history();

  tor_asprintf(&out,
               "bridge-stats-end %s (%ld s)\n"
               "bridge-ips %s\n"
               "bridge-ip-versions %s\n"
               "bridge-ip-transports %s\n",
               written, duration,
               country_data ? country_data : "",
               ipver_data ? ipver_data : "",
               transport_data ? transport_data : "");
  tor_free(country_data);
  tor_free(ipver_data);
  tor_free(transport_data);

  return out;
}

/** Return a newly allocated string holding our bridge usage stats by country
 * in a format suitable for the answer to a controller request. Return NULL on
 * failure.  */
static char *
format_bridge_stats_controller(time_t now)
{
  char *out = NULL, *country_data = NULL, *ipver_data = NULL;
  char started[ISO_TIME_LEN+1];
  (void) now;

  format_iso_time(started, start_of_bridge_stats_interval);
  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &country_data, &ipver_data);

  tor_asprintf(&out,
               "TimeStarted=\"%s\" CountrySummary=%s IPVersions=%s",
               started,
               country_data ? country_data : "",
               ipver_data ? ipver_data : "");
  tor_free(country_data);
  tor_free(ipver_data);
  return out;
}

/** Write bridge statistics to $DATADIR/stats/bridge-stats and return
 * when we should next try to write statistics. */
time_t
geoip_bridge_stats_write(time_t now)
{
  char *val = NULL;

  /* Check if 24 hours have passed since starting measurements. */
  if (now < start_of_bridge_stats_interval + WRITE_STATS_INTERVAL)
    return start_of_bridge_stats_interval + WRITE_STATS_INTERVAL;

  /* Discard all items in the client history that are too old. */
  geoip_remove_old_clients(start_of_bridge_stats_interval);

  /* Generate formatted string */
  val = geoip_format_bridge_stats(now);
  if (val == NULL)
    goto done;

  /* Update the stored value. */
  tor_free(bridge_stats_extrainfo);
  bridge_stats_extrainfo = val;
  start_of_bridge_stats_interval = now;

  /* Write it to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "bridge-stats",
                         bridge_stats_extrainfo, "bridge statistics");

    /* Tell the controller, "hey, there are clients!" */
    {
      char *controller_str = format_bridge_stats_controller(now);
      if (controller_str)
        control_event_clients_seen(controller_str);
      tor_free(controller_str);
    }
  }

 done:
  return start_of_bridge_stats_interval + WRITE_STATS_INTERVAL;
}

/** Try to load the most recent bridge statistics from disk, unless we
 * have finished a measurement interval lately, and check whether they
 * are still recent enough. */
static void
load_bridge_stats(time_t now)
{
  char *fname, *contents;
  if (bridge_stats_extrainfo)
    return;

  fname = get_datadir_fname2("stats", "bridge-stats");
  contents = read_file_to_str(fname, RFTS_IGNORE_MISSING, NULL);
  if (contents && validate_bridge_stats(contents, now)) {
    bridge_stats_extrainfo = contents;
  } else {
    tor_free(contents);
  }

  tor_free(fname);
}

/** Return most recent bridge statistics for inclusion in extra-info
 * descriptors, or NULL if we don't have recent bridge statistics. */
const char *
geoip_get_bridge_stats_extrainfo(time_t now)
{
  load_bridge_stats(now);
  return bridge_stats_extrainfo;
}

/** Return a new string containing the recent bridge statistics to be returned
 * to controller clients, or NULL if we don't have any bridge statistics. */
char *
geoip_get_bridge_stats_controller(time_t now)
{
  return format_bridge_stats_controller(now);
}

/** Start time of entry stats or 0 if we're not collecting entry
 * statistics. */
static time_t start_of_entry_stats_interval;

/** Initialize entry stats. */
void
geoip_entry_stats_init(time_t now)
{
  start_of_entry_stats_interval = now;
}

/** Reset counters for entry stats. */
void
geoip_reset_entry_stats(time_t now)
{
  client_history_clear();
  start_of_entry_stats_interval = now;
}

/** Stop collecting entry stats in a way that we can re-start doing so in
 * geoip_entry_stats_init(). */
void
geoip_entry_stats_term(void)
{
  geoip_reset_entry_stats(0);
}

/** Return a newly allocated string containing the entry statistics
 * until <b>now</b>, or NULL if we're not collecting entry stats. Caller
 * must ensure start_of_entry_stats_interval lies in the past. */
char *
geoip_format_entry_stats(time_t now)
{
  char t[ISO_TIME_LEN+1];
  char *data = NULL;
  char *result;

  if (!start_of_entry_stats_interval)
    return NULL; /* Not initialized. */

  tor_assert(now >= start_of_entry_stats_interval);

  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &data, NULL);
  format_iso_time(t, now);
  tor_asprintf(&result,
               "entry-stats-end %s (%u s)\n"
               "entry-ips %s\n",
               t, (unsigned) (now - start_of_entry_stats_interval),
               data ? data : "");
  tor_free(data);
  return result;
}

/** If 24 hours have passed since the beginning of the current entry stats
 * period, write entry stats to $DATADIR/stats/entry-stats (possibly
 * overwriting an existing file) and reset counters.  Return when we would
 * next want to write entry stats or 0 if we never want to write. */
time_t
geoip_entry_stats_write(time_t now)
{
  char *str = NULL;

  if (!start_of_entry_stats_interval)
    return 0; /* Not initialized. */
  if (start_of_entry_stats_interval + WRITE_STATS_INTERVAL > now)
    goto done; /* Not ready to write. */

  /* Discard all items in the client history that are too old. */
  geoip_remove_old_clients(start_of_entry_stats_interval);

  /* Generate history string .*/
  str = geoip_format_entry_stats(now);

  /* Write entry-stats string to disk. */
  if (!check_or_create_data_subdir("stats")) {
    write_to_data_subdir("stats", "entry-stats", str, "entry statistics");

    /* Reset measurement interval start. */
    geoip_reset_entry_stats(now);
  }

 done:
  tor_free(str);
  return start_of_entry_stats_interval + WRITE_STATS_INTERVAL;
}

/** Helper used to implement GETINFO ip-to-country/... controller command. */
int
getinfo_helper_geoip(control_connection_t *control_conn,
                     const char *question, char **answer,
                     const char **errmsg)
{
  (void)control_conn;
  if (!strcmpstart(question, "ip-to-country/")) {
    int c;
    sa_family_t family;
    tor_addr_t addr;
    question += strlen("ip-to-country/");
    family = tor_addr_parse(&addr, question);
    if (family != AF_INET && family != AF_INET6) {
      *errmsg = "Invalid address family";
      return -1;
    }
    if (!geoip_is_loaded(family)) {
      *errmsg = "GeoIP data not loaded";
      return -1;
    }
    if (family == AF_INET)
      c = geoip_get_country_by_ipv4(tor_addr_to_ipv4h(&addr));
    else                      /* AF_INET6 */
      c = geoip_get_country_by_ipv6(tor_addr_to_in6(&addr));
    *answer = tor_strdup(geoip_get_country_name(c));
  }
  return 0;
}

/** Release all storage held by the GeoIP databases and country list. */
static void
clear_geoip_db(void)
{
  if (geoip_countries) {
    SMARTLIST_FOREACH(geoip_countries, geoip_country_t *, c, tor_free(c));
    smartlist_free(geoip_countries);
  }

  strmap_free(country_idxplus1_by_lc_code, NULL);
  if (geoip_ipv4_entries) {
    SMARTLIST_FOREACH(geoip_ipv4_entries, geoip_ipv4_entry_t *, ent,
                      tor_free(ent));
    smartlist_free(geoip_ipv4_entries);
  }
  if (geoip_ipv6_entries) {
    SMARTLIST_FOREACH(geoip_ipv6_entries, geoip_ipv6_entry_t *, ent,
                      tor_free(ent));
    smartlist_free(geoip_ipv6_entries);
  }
  geoip_countries = NULL;
  country_idxplus1_by_lc_code = NULL;
  geoip_ipv4_entries = NULL;
  geoip_ipv6_entries = NULL;
}

/** Release all storage held in this file. */
void
geoip_free_all(void)
{
  {
    clientmap_entry_t **ent, **next, *this;
    for (ent = HT_START(clientmap, &client_history); ent != NULL; ent = next) {
      this = *ent;
      next = HT_NEXT_RMV(clientmap, &client_history, ent);
      clientmap_entry_free(this);
    }
    HT_CLEAR(clientmap, &client_history);
  }
  {
    dirreq_map_entry_t **ent, **next, *this;
    for (ent = HT_START(dirreqmap, &dirreq_map); ent != NULL; ent = next) {
      this = *ent;
      next = HT_NEXT_RMV(dirreqmap, &dirreq_map, ent);
      tor_free(this);
    }
    HT_CLEAR(dirreqmap, &dirreq_map);
  }

  clear_geoip_db();
  tor_free(bridge_stats_extrainfo);
}

