/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file protover.c
 * \brief Versioning information for different pieces of the Tor protocol.
 *
 * Starting in version 0.2.9.3-alpha, Tor places separate version numbers on
 * each of the different components of its protocol. Relays use these numbers
 * to advertise what versions of the protocols they can support, and clients
 * use them to find what they can ask a given relay to do.  Authorities vote
 * on the supported protocol versions for each relay, and also vote on the
 * which protocols you should have to support in order to be on the Tor
 * network. All Tor instances use these required/recommended protocol versions
 * to tell what level of support for recent protocols each relay has, and
 * to decide whether they should be running given their current protocols.
 *
 * The main advantage of these protocol versions numbers over using Tor
 * version numbers is that they allow different implementations of the Tor
 * protocols to develop independently, without having to claim compatibility
 * with specific versions of Tor.
 **/

#define PROTOVER_PRIVATE

#include "or.h"
#include "protover.h"
#include "routerparse.h"

static const smartlist_t *get_supported_protocol_list(void);
static int protocol_list_contains(const smartlist_t *protos,
                                  protocol_type_t pr, uint32_t ver);

/** Mapping between protocol type string and protocol type. */
static const struct {
  protocol_type_t protover_type;
  const char *name;
} PROTOCOL_NAMES[] = {
  { PRT_LINK, "Link" },
  { PRT_LINKAUTH, "LinkAuth" },
  { PRT_RELAY, "Relay" },
  { PRT_DIRCACHE, "DirCache" },
  { PRT_HSDIR, "HSDir" },
  { PRT_HSINTRO, "HSIntro" },
  { PRT_HSREND, "HSRend" },
  { PRT_DESC, "Desc" },
  { PRT_MICRODESC, "Microdesc"},
  { PRT_CONS, "Cons" }
};

#define N_PROTOCOL_NAMES ARRAY_LENGTH(PROTOCOL_NAMES)

/**
 * Given a protocol_type_t, return the corresponding string used in
 * descriptors.
 */
STATIC const char *
protocol_type_to_str(protocol_type_t pr)
{
  unsigned i;
  for (i=0; i < N_PROTOCOL_NAMES; ++i) {
    if (PROTOCOL_NAMES[i].protover_type == pr)
      return PROTOCOL_NAMES[i].name;
  }
  /* LCOV_EXCL_START */
  tor_assert_nonfatal_unreached_once();
  return "UNKNOWN";
  /* LCOV_EXCL_STOP */
}

/**
 * Given a string, find the corresponding protocol type and store it in
 * <b>pr_out</b>. Return 0 on success, -1 on failure.
 */
STATIC int
str_to_protocol_type(const char *s, protocol_type_t *pr_out)
{
  if (BUG(!pr_out))
    return -1;

  unsigned i;
  for (i=0; i < N_PROTOCOL_NAMES; ++i) {
    if (0 == strcmp(s, PROTOCOL_NAMES[i].name)) {
      *pr_out = PROTOCOL_NAMES[i].protover_type;
      return 0;
    }
  }

  return -1;
}

/**
 * Release all space held by a single proto_entry_t structure
 */
STATIC void
proto_entry_free(proto_entry_t *entry)
{
  if (!entry)
    return;
  tor_free(entry->name);
  SMARTLIST_FOREACH(entry->ranges, proto_range_t *, r, tor_free(r));
  smartlist_free(entry->ranges);
  tor_free(entry);
}

/**
 * Given a string <b>s</b> and optional end-of-string pointer
 * <b>end_of_range</b>, parse the protocol range and store it in
 * <b>low_out</b> and <b>high_out</b>.  A protocol range has the format U, or
 * U-U, where U is an unsigned 32-bit integer.
 */
static int
parse_version_range(const char *s, const char *end_of_range,
                    uint32_t *low_out, uint32_t *high_out)
{
  uint32_t low, high;
  char *next = NULL;
  int ok;

  tor_assert(high_out);
  tor_assert(low_out);

  if (BUG(!end_of_range))
    end_of_range = s + strlen(s); // LCOV_EXCL_LINE

  /* Note that this wouldn't be safe if we didn't know that eventually,
   * we'd hit a NUL */
  low = (uint32_t) tor_parse_ulong(s, 10, 0, UINT32_MAX, &ok, &next);
  if (!ok)
    goto error;
  if (next > end_of_range)
    goto error;
  if (next == end_of_range) {
    high = low;
    goto done;
  }

  if (*next != '-')
    goto error;
  s = next+1;
  /* ibid */
  high = (uint32_t) tor_parse_ulong(s, 10, 0, UINT32_MAX, &ok, &next);
  if (!ok)
    goto error;
  if (next != end_of_range)
    goto error;

 done:
  *high_out = high;
  *low_out = low;
  return 0;

 error:
  return -1;
}

/** Parse a single protocol entry from <b>s</b> up to an optional
 * <b>end_of_entry</b> pointer, and return that protocol entry. Return NULL
 * on error.
 *
 * A protocol entry has a keyword, an = sign, and zero or more ranges. */
static proto_entry_t *
parse_single_entry(const char *s, const char *end_of_entry)
{
  proto_entry_t *out = tor_malloc_zero(sizeof(proto_entry_t));
  const char *equals;

  out->ranges = smartlist_new();

  if (BUG (!end_of_entry))
    end_of_entry = s + strlen(s); // LCOV_EXCL_LINE

  /* There must be an =. */
  equals = memchr(s, '=', end_of_entry - s);
  if (!equals)
    goto error;

  /* The name must be nonempty */
  if (equals == s)
    goto error;

  out->name = tor_strndup(s, equals-s);

  tor_assert(equals < end_of_entry);

  s = equals + 1;
  while (s < end_of_entry) {
    const char *comma = memchr(s, ',', end_of_entry-s);
    proto_range_t *range = tor_malloc_zero(sizeof(proto_range_t));
    if (! comma)
      comma = end_of_entry;

    smartlist_add(out->ranges, range);
    if (parse_version_range(s, comma, &range->low, &range->high) < 0) {
      goto error;
    }

    if (range->low > range->high) {
      goto error;
    }

    s = comma;
    while (*s == ',' && s < end_of_entry)
      ++s;
  }

  return out;

 error:
  proto_entry_free(out);
  return NULL;
}

/**
 * Parse the protocol list from <b>s</b> and return it as a smartlist of
 * proto_entry_t
 */
STATIC smartlist_t *
parse_protocol_list(const char *s)
{
  smartlist_t *entries = smartlist_new();

  while (*s) {
    /* Find the next space or the NUL. */
    const char *end_of_entry = strchr(s, ' ');
    proto_entry_t *entry;
    if (!end_of_entry)
      end_of_entry = s + strlen(s);

    entry = parse_single_entry(s, end_of_entry);

    if (! entry)
      goto error;

    smartlist_add(entries, entry);

    s = end_of_entry;
    while (*s == ' ')
      ++s;
  }

  return entries;

 error:
  SMARTLIST_FOREACH(entries, proto_entry_t *, ent, proto_entry_free(ent));
  smartlist_free(entries);
  return NULL;
}

/**
 * Given a protocol type and version number, return true iff we know
 * how to speak that protocol.
 */
int
protover_is_supported_here(protocol_type_t pr, uint32_t ver)
{
  const smartlist_t *ours = get_supported_protocol_list();
  return protocol_list_contains(ours, pr, ver);
}

/**
 * Return true iff "list" encodes a protocol list that includes support for
 * the indicated protocol and version.
 */
int
protocol_list_supports_protocol(const char *list, protocol_type_t tp,
                                uint32_t version)
{
  /* NOTE: This is a pretty inefficient implementation. If it ever shows
   * up in profiles, we should memoize it.
   */
  smartlist_t *protocols = parse_protocol_list(list);
  if (!protocols) {
    return 0;
  }
  int contains = protocol_list_contains(protocols, tp, version);

  SMARTLIST_FOREACH(protocols, proto_entry_t *, ent, proto_entry_free(ent));
  smartlist_free(protocols);
  return contains;
}

/** Return the canonical string containing the list of protocols
 * that we support. */
const char *
protover_get_supported_protocols(void)
{
  return
    "Cons=1-2 "
    "Desc=1-2 "
    "DirCache=1 "
    "HSDir=1 "
    "HSIntro=3 "
    "HSRend=1-2 "
    "Link=1-4 "
    "LinkAuth=1 "
    "Microdesc=1-2 "
    "Relay=1-2";
}

/** The protocols from protover_get_supported_protocols(), as parsed into a
 * list of proto_entry_t values. Access this via
 * get_supported_protocol_list. */
static smartlist_t *supported_protocol_list = NULL;

/** Return a pointer to a smartlist of proto_entry_t for the protocols
 * we support. */
static const smartlist_t *
get_supported_protocol_list(void)
{
  if (PREDICT_UNLIKELY(supported_protocol_list == NULL)) {
    supported_protocol_list =
      parse_protocol_list(protover_get_supported_protocols());
  }
  return supported_protocol_list;
}

/**
 * Given a protocol entry, encode it at the end of the smartlist <b>chunks</b>
 * as one or more newly allocated strings.
 */
static void
proto_entry_encode_into(smartlist_t *chunks, const proto_entry_t *entry)
{
  smartlist_add_asprintf(chunks, "%s=", entry->name);

  SMARTLIST_FOREACH_BEGIN(entry->ranges, proto_range_t *, range) {
    const char *comma = "";
    if (range_sl_idx != 0)
      comma = ",";

    if (range->low == range->high) {
      smartlist_add_asprintf(chunks, "%s%lu",
                             comma, (unsigned long)range->low);
    } else {
      smartlist_add_asprintf(chunks, "%s%lu-%lu",
                             comma, (unsigned long)range->low,
                             (unsigned long)range->high);
    }
  } SMARTLIST_FOREACH_END(range);
}

/** Given a list of space-separated proto_entry_t items,
 * encode it into a newly allocated space-separated string. */
STATIC char *
encode_protocol_list(const smartlist_t *sl)
{
  const char *separator = "";
  smartlist_t *chunks = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(sl, const proto_entry_t *, ent) {
    smartlist_add(chunks, tor_strdup(separator));

    proto_entry_encode_into(chunks, ent);

    separator = " ";
  } SMARTLIST_FOREACH_END(ent);

  char *result = smartlist_join_strings(chunks, "", 0, NULL);

  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);

  return result;
}

/* We treat any protocol list with more than this many subprotocols in it
 * as a DoS attempt. */
static const int MAX_PROTOCOLS_TO_EXPAND = (1<<16);

/** Voting helper: Given a list of proto_entry_t, return a newly allocated
 * smartlist of newly allocated strings, one for each included protocol
 * version. (So 'Foo=3,5-7' expands to a list of 'Foo=3', 'Foo=5', 'Foo=6',
 * 'Foo=7'.)
 *
 * Do not list any protocol version more than once.
 *
 * Return NULL if the list would be too big.
 */
static smartlist_t *
expand_protocol_list(const smartlist_t *protos)
{
  smartlist_t *expanded = smartlist_new();
  if (!protos)
    return expanded;

  SMARTLIST_FOREACH_BEGIN(protos, const proto_entry_t *, ent) {
    const char *name = ent->name;
    SMARTLIST_FOREACH_BEGIN(ent->ranges, const proto_range_t *, range) {
      uint32_t u;
      for (u = range->low; u <= range->high; ++u) {
        smartlist_add_asprintf(expanded, "%s=%lu", name, (unsigned long)u);
        if (smartlist_len(expanded) > MAX_PROTOCOLS_TO_EXPAND)
          goto too_many;
      }
    } SMARTLIST_FOREACH_END(range);
  } SMARTLIST_FOREACH_END(ent);

  smartlist_sort_strings(expanded);
  smartlist_uniq_strings(expanded); // This makes voting work. do not remove
  return expanded;

 too_many:
  SMARTLIST_FOREACH(expanded, char *, cp, tor_free(cp));
  smartlist_free(expanded);
  return NULL;
}

/** Voting helper: compare two singleton proto_entry_t items by version
 * alone. (A singleton item is one with a single range entry where
 * low==high.) */
static int
cmp_single_ent_by_version(const void **a_, const void **b_)
{
  const proto_entry_t *ent_a = *a_;
  const proto_entry_t *ent_b = *b_;

  tor_assert(smartlist_len(ent_a->ranges) == 1);
  tor_assert(smartlist_len(ent_b->ranges) == 1);

  const proto_range_t *a = smartlist_get(ent_a->ranges, 0);
  const proto_range_t *b = smartlist_get(ent_b->ranges, 0);

  tor_assert(a->low == a->high);
  tor_assert(b->low == b->high);

  if (a->low < b->low) {
    return -1;
  } else if (a->low == b->low) {
    return 0;
  } else {
    return 1;
  }
}

/** Voting helper: Given a list of singleton protocol strings (of the form
 * Foo=7), return a canonical listing of all the protocol versions listed,
 * with as few ranges as possible, with protocol versions sorted lexically and
 * versions sorted in numerically increasing order, using as few range entries
 * as possible.
 **/
static char *
contract_protocol_list(const smartlist_t *proto_strings)
{
  // map from name to list of single-version entries
  strmap_t *entry_lists_by_name = strmap_new();
  // list of protocol names
  smartlist_t *all_names = smartlist_new();
  // list of strings for the output we're building
  smartlist_t *chunks = smartlist_new();

  // Parse each item and stick it entry_lists_by_name. Build
  // 'all_names' at the same time.
  SMARTLIST_FOREACH_BEGIN(proto_strings, const char *, s) {
    if (BUG(!s))
      continue;// LCOV_EXCL_LINE
    proto_entry_t *ent = parse_single_entry(s, s+strlen(s));
    if (BUG(!ent))
      continue; // LCOV_EXCL_LINE
    smartlist_t *lst = strmap_get(entry_lists_by_name, ent->name);
    if (!lst) {
      smartlist_add(all_names, ent->name);
      lst = smartlist_new();
      strmap_set(entry_lists_by_name, ent->name, lst);
    }
    smartlist_add(lst, ent);
  } SMARTLIST_FOREACH_END(s);

  // We want to output the protocols sorted by their name.
  smartlist_sort_strings(all_names);

  SMARTLIST_FOREACH_BEGIN(all_names, const char *, name) {
    const int first_entry = (name_sl_idx == 0);
    smartlist_t *lst = strmap_get(entry_lists_by_name, name);
    tor_assert(lst);
    // Sort every entry with this name by version. They are
    // singletons, so there can't be overlap.
    smartlist_sort(lst, cmp_single_ent_by_version);

    if (! first_entry)
      smartlist_add(chunks, tor_strdup(" "));

    /* We're going to construct this entry from the ranges. */
    proto_entry_t *entry = tor_malloc_zero(sizeof(proto_entry_t));
    entry->ranges = smartlist_new();
    entry->name = tor_strdup(name);

    // Now, find all the ranges of versions start..end where
    // all of start, start+1, start+2, ..end are included.
    int start_of_cur_series = 0;
    while (start_of_cur_series < smartlist_len(lst)) {
      const proto_entry_t *ent = smartlist_get(lst, start_of_cur_series);
      const proto_range_t *range = smartlist_get(ent->ranges, 0);
      const uint32_t ver_low = range->low;
      uint32_t ver_high = ver_low;

      int idx;
      for (idx = start_of_cur_series+1; idx < smartlist_len(lst); ++idx) {
        ent = smartlist_get(lst, idx);
        range = smartlist_get(ent->ranges, 0);
        if (range->low != ver_high + 1)
          break;
        ver_high += 1;
      }

      // Now idx is either off the end of the list, or the first sequence
      // break in the list.
      start_of_cur_series = idx;

      proto_range_t *new_range = tor_malloc_zero(sizeof(proto_range_t));
      new_range->low = ver_low;
      new_range->high = ver_high;
      smartlist_add(entry->ranges, new_range);
    }
    proto_entry_encode_into(chunks, entry);
    proto_entry_free(entry);

  } SMARTLIST_FOREACH_END(name);

  // Build the result...
  char *result = smartlist_join_strings(chunks, "", 0, NULL);

  // And free all the stuff we allocated.
  SMARTLIST_FOREACH_BEGIN(all_names, const char *, name) {
    smartlist_t *lst = strmap_get(entry_lists_by_name, name);
    tor_assert(lst);
    SMARTLIST_FOREACH(lst, proto_entry_t *, e, proto_entry_free(e));
    smartlist_free(lst);
  } SMARTLIST_FOREACH_END(name);

  strmap_free(entry_lists_by_name, NULL);
  smartlist_free(all_names);
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);

  return result;
}

/**
 * Protocol voting implementation.
 *
 * Given a list of strings describing protocol versions, return a newly
 * allocated string encoding all of the protocols that are listed by at
 * least <b>threshold</b> of the inputs.
 *
 * The string is minimal and sorted according to the rules of
 * contract_protocol_list above.
 */
char *
protover_compute_vote(const smartlist_t *list_of_proto_strings,
                      int threshold)
{
  smartlist_t *all_entries = smartlist_new();

  // First, parse the inputs and break them into singleton entries.
  SMARTLIST_FOREACH_BEGIN(list_of_proto_strings, const char *, vote) {
    smartlist_t *unexpanded = parse_protocol_list(vote);
    smartlist_t *this_vote = expand_protocol_list(unexpanded);
    if (this_vote == NULL) {
      log_warn(LD_NET, "When expanding a protocol list from an authority, I "
               "got too many protocols. This is possibly an attack or a bug, "
               "unless the Tor network truly has expanded to support over %d "
               "different subprotocol versions. The offending string was: %s",
               MAX_PROTOCOLS_TO_EXPAND, escaped(vote));
    } else {
      smartlist_add_all(all_entries, this_vote);
      smartlist_free(this_vote);
    }
    SMARTLIST_FOREACH(unexpanded, proto_entry_t *, e, proto_entry_free(e));
    smartlist_free(unexpanded);
  } SMARTLIST_FOREACH_END(vote);

  // Now sort the singleton entries
  smartlist_sort_strings(all_entries);

  // Now find all the strings that appear at least 'threshold' times.
  smartlist_t *include_entries = smartlist_new();
  const char *cur_entry = smartlist_get(all_entries, 0);
  int n_times = 0;
  SMARTLIST_FOREACH_BEGIN(all_entries, const char *, ent) {
    if (!strcmp(ent, cur_entry)) {
      n_times++;
    } else {
      if (n_times >= threshold && cur_entry)
        smartlist_add(include_entries, (void*)cur_entry);
      cur_entry = ent;
      n_times = 1 ;
    }
  } SMARTLIST_FOREACH_END(ent);

  if (n_times >= threshold && cur_entry)
    smartlist_add(include_entries, (void*)cur_entry);

  // Finally, compress that list.
  char *result = contract_protocol_list(include_entries);
  smartlist_free(include_entries);
  SMARTLIST_FOREACH(all_entries, char *, cp, tor_free(cp));
  smartlist_free(all_entries);

  return result;
}

/** Return true if every protocol version described in the string <b>s</b> is
 * one that we support, and false otherwise.  If <b>missing_out</b> is
 * provided, set it to the list of protocols we do not support.
 *
 * NOTE: This is quadratic, but we don't do it much: only a few times per
 * consensus. Checking signatures should be way more expensive than this
 * ever would be.
 **/
int
protover_all_supported(const char *s, char **missing_out)
{
  int all_supported = 1;
  smartlist_t *missing;

  if (!s) {
    return 1;
  }

  smartlist_t *entries = parse_protocol_list(s);

  missing = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(entries, const proto_entry_t *, ent) {
    protocol_type_t tp;
    if (str_to_protocol_type(ent->name, &tp) < 0) {
      if (smartlist_len(ent->ranges)) {
        goto unsupported;
      }
      continue;
    }

    SMARTLIST_FOREACH_BEGIN(ent->ranges, const proto_range_t *, range) {
      uint32_t i;
      for (i = range->low; i <= range->high; ++i) {
        if (!protover_is_supported_here(tp, i)) {
          goto unsupported;
        }
      }
    } SMARTLIST_FOREACH_END(range);

    continue;

  unsupported:
    all_supported = 0;
    smartlist_add(missing, (void*) ent);
  } SMARTLIST_FOREACH_END(ent);

  if (missing_out && !all_supported) {
    tor_assert(0 != smartlist_len(missing));
    *missing_out = encode_protocol_list(missing);
  }
  smartlist_free(missing);

  SMARTLIST_FOREACH(entries, proto_entry_t *, ent, proto_entry_free(ent));
  smartlist_free(entries);

  return all_supported;
}

/** Helper: Given a list of proto_entry_t, return true iff
 * <b>pr</b>=<b>ver</b> is included in that list. */
static int
protocol_list_contains(const smartlist_t *protos,
                       protocol_type_t pr, uint32_t ver)
{
  if (BUG(protos == NULL)) {
    return 0; // LCOV_EXCL_LINE
  }
  const char *pr_name = protocol_type_to_str(pr);
  if (BUG(pr_name == NULL)) {
    return 0; // LCOV_EXCL_LINE
  }

  SMARTLIST_FOREACH_BEGIN(protos, const proto_entry_t *, ent) {
    if (strcasecmp(ent->name, pr_name))
      continue;
    /* name matches; check the ranges */
    SMARTLIST_FOREACH_BEGIN(ent->ranges, const proto_range_t *, range) {
      if (ver >= range->low && ver <= range->high)
        return 1;
    } SMARTLIST_FOREACH_END(range);
  } SMARTLIST_FOREACH_END(ent);

  return 0;
}

/** Return a string describing the protocols supported by tor version
 * <b>version</b>, or an empty string if we cannot tell.
 *
 * Note that this is only used to infer protocols for Tor versions that
 * can't declare their own.
 **/
const char *
protover_compute_for_old_tor(const char *version)
{
  if (tor_version_as_new_as(version,
                            FIRST_TOR_VERSION_TO_ADVERTISE_PROTOCOLS)) {
    return "";
  } else if (tor_version_as_new_as(version, "0.2.9.1-alpha")) {
    /* 0.2.9.1-alpha HSRend=2 */
    return "Cons=1-2 Desc=1-2 DirCache=1 HSDir=1 HSIntro=3 HSRend=1-2 "
      "Link=1-4 LinkAuth=1 "
      "Microdesc=1-2 Relay=1-2";
  } else if (tor_version_as_new_as(version, "0.2.7.5")) {
    /* 0.2.7-stable added Desc=2, Microdesc=2, Cons=2, which indicate
     * ed25519 support.  We'll call them present only in "stable" 027,
     * though. */
    return "Cons=1-2 Desc=1-2 DirCache=1 HSDir=1 HSIntro=3 HSRend=1 "
      "Link=1-4 LinkAuth=1 "
      "Microdesc=1-2 Relay=1-2";
  } else if (tor_version_as_new_as(version, "0.2.4.19")) {
    /* No currently supported Tor server versions are older than this, or
     * lack these protocols. */
    return "Cons=1 Desc=1 DirCache=1 HSDir=1 HSIntro=3 HSRend=1 "
      "Link=1-4 LinkAuth=1 "
      "Microdesc=1 Relay=1-2";
  } else {
    /* Cannot infer protocols. */
    return "";
  }
}

/**
 * Release all storage held by static fields in protover.c
 */
void
protover_free_all(void)
{
  if (supported_protocol_list) {
    smartlist_t *entries = supported_protocol_list;
    SMARTLIST_FOREACH(entries, proto_entry_t *, ent, proto_entry_free(ent));
    smartlist_free(entries);
    supported_protocol_list = NULL;
  }
}

