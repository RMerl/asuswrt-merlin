/* Copyright (c) 2003-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file dns.c
 * \brief Implements a local cache for DNS results for Tor servers.
 * This is implemented as a wrapper around Adam Langley's eventdns.c code.
 * (We can't just use gethostbyname() and friends because we really need to
 * be nonblocking.)
 **/

#include "or.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "control.h"
#include "dns.h"
#include "main.h"
#include "policies.h"
#include "relay.h"
#include "router.h"
#include "ht.h"
#include "../common/sandbox.h"
#ifdef HAVE_EVENT2_DNS_H
#include <event2/event.h>
#include <event2/dns.h>
#else
#include <event.h>
#include "eventdns.h"
#ifndef HAVE_EVDNS_SET_DEFAULT_OUTGOING_BIND_ADDRESS
#define HAVE_EVDNS_SET_DEFAULT_OUTGOING_BIND_ADDRESS
#endif
#endif

#ifndef HAVE_EVENT2_DNS_H
struct evdns_base;
struct evdns_request;
#define evdns_base_new(x,y) tor_malloc(1)
#define evdns_base_clear_nameservers_and_suspend(base) \
  evdns_clear_nameservers_and_suspend()
#define evdns_base_search_clear(base) evdns_search_clear()
#define evdns_base_set_default_outgoing_bind_address(base, a, len)  \
  evdns_set_default_outgoing_bind_address((a),(len))
#define evdns_base_resolv_conf_parse(base, options, fname) \
  evdns_resolv_conf_parse((options), (fname))
#define evdns_base_count_nameservers(base)      \
  evdns_count_nameservers()
#define evdns_base_resume(base)                 \
  evdns_resume()
#define evdns_base_config_windows_nameservers(base)     \
  evdns_config_windows_nameservers()
#define evdns_base_set_option_(base, opt, val) \
  evdns_set_option((opt),(val),DNS_OPTIONS_ALL)
/* Note: our internal eventdns.c, plus Libevent 1.4, used a 1 return to
 * signify failure to launch a resolve. Libevent 2.0 uses a -1 return to
 * signify a failure on a resolve, though if we're on Libevent 2.0, we should
 * have event2/dns.h and never hit these macros.  Regardless, 0 is success. */
#define evdns_base_resolve_ipv4(base, addr, options, cb, ptr) \
  ((evdns_resolve_ipv4((addr), (options), (cb), (ptr))!=0)    \
   ? NULL : ((void*)1))
#define evdns_base_resolve_ipv6(base, addr, options, cb, ptr) \
  ((evdns_resolve_ipv6((addr), (options), (cb), (ptr))!=0)    \
   ? NULL : ((void*)1))
#define evdns_base_resolve_reverse(base, addr, options, cb, ptr)        \
  ((evdns_resolve_reverse((addr), (options), (cb), (ptr))!=0)           \
   ? NULL : ((void*)1))
#define evdns_base_resolve_reverse_ipv6(base, addr, options, cb, ptr)   \
  ((evdns_resolve_reverse_ipv6((addr), (options), (cb), (ptr))!=0)      \
   ? NULL : ((void*)1))

#elif defined(LIBEVENT_VERSION_NUMBER) && LIBEVENT_VERSION_NUMBER < 0x02000303
#define evdns_base_set_option_(base, opt, val) \
  evdns_base_set_option((base), (opt),(val),DNS_OPTIONS_ALL)

#else
#define evdns_base_set_option_ evdns_base_set_option

#endif

/** Longest hostname we're willing to resolve. */
#define MAX_ADDRESSLEN 256

/** How long will we wait for an answer from the resolver before we decide
 * that the resolver is wedged? */
#define RESOLVE_MAX_TIMEOUT 300

/** Our evdns_base; this structure handles all our name lookups. */
static struct evdns_base *the_evdns_base = NULL;

/** Have we currently configured nameservers with eventdns? */
static int nameservers_configured = 0;
/** Did our most recent attempt to configure nameservers with eventdns fail? */
static int nameserver_config_failed = 0;
/** What was the resolv_conf fname we last used when configuring the
 * nameservers? Used to check whether we need to reconfigure. */
static char *resolv_conf_fname = NULL;
/** What was the mtime on the resolv.conf file we last used when configuring
 * the nameservers?  Used to check whether we need to reconfigure. */
static time_t resolv_conf_mtime = 0;

/** Linked list of connections waiting for a DNS answer. */
typedef struct pending_connection_t {
  edge_connection_t *conn;
  struct pending_connection_t *next;
} pending_connection_t;

/** Value of 'magic' field for cached_resolve_t.  Used to try to catch bad
 * pointers and memory stomping. */
#define CACHED_RESOLVE_MAGIC 0x1234F00D

/* Possible states for a cached resolve_t */
/** We are waiting for the resolver system to tell us an answer here.
 * When we get one, or when we time out, the state of this cached_resolve_t
 * will become "DONE" and we'll possibly add a CACHED
 * entry. This cached_resolve_t will be in the hash table so that we will
 * know not to launch more requests for this addr, but rather to add more
 * connections to the pending list for the addr. */
#define CACHE_STATE_PENDING 0
/** This used to be a pending cached_resolve_t, and we got an answer for it.
 * Now we're waiting for this cached_resolve_t to expire.  This should
 * have no pending connections, and should not appear in the hash table. */
#define CACHE_STATE_DONE 1
/** We are caching an answer for this address. This should have no pending
 * connections, and should appear in the hash table. */
#define CACHE_STATE_CACHED 2

/** @name status values for a single DNS request.
 *
 * @{ */
/** The DNS request is in progress. */
#define RES_STATUS_INFLIGHT 1
/** The DNS request finished and gave an answer */
#define RES_STATUS_DONE_OK 2
/** The DNS request finished and gave an error */
#define RES_STATUS_DONE_ERR 3
/**@}*/

/** A DNS request: possibly completed, possibly pending; cached_resolve
 * structs are stored at the OR side in a hash table, and as a linked
 * list from oldest to newest.
 */
typedef struct cached_resolve_t {
  HT_ENTRY(cached_resolve_t) node;
  uint32_t magic;  /**< Must be CACHED_RESOLVE_MAGIC */
  char address[MAX_ADDRESSLEN]; /**< The hostname to be resolved. */

  union {
    uint32_t addr_ipv4; /**< IPv4 addr for <b>address</b>, if successful.
                         * (In host order.) */
    int err_ipv4; /**< One of DNS_ERR_*, if IPv4 lookup failed. */
  } result_ipv4; /**< Outcome of IPv4 lookup */
  union {
    struct in6_addr addr_ipv6; /**< IPv6 addr for <b>address</b>, if
                                * successful */
    int err_ipv6; /**< One of DNS_ERR_*, if IPv6 lookup failed. */
  } result_ipv6; /**< Outcome of IPv6 lookup, if any */
  union {
    char *hostname; /** A hostname, if PTR lookup happened successfully*/
    int err_hostname; /** One of DNS_ERR_*, if PTR lookup failed. */
  } result_ptr;
  /** @name Status fields
   *
   * These take one of the RES_STATUS_* values, depending on the state
   * of the corresponding lookup.
   *
   * @{ */
  unsigned int res_status_ipv4 : 2;
  unsigned int res_status_ipv6 : 2;
  unsigned int res_status_hostname : 2;
  /**@}*/
  uint8_t state; /**< Is this cached entry pending/done/informative? */

  time_t expire; /**< Remove items from cache after this time. */
  uint32_t ttl_ipv4; /**< What TTL did the nameserver tell us? */
  uint32_t ttl_ipv6; /**< What TTL did the nameserver tell us? */
  uint32_t ttl_hostname; /**< What TTL did the nameserver tell us? */
  /** Connections that want to know when we get an answer for this resolve. */
  pending_connection_t *pending_connections;
  /** Position of this element in the heap*/
  int minheap_idx;
} cached_resolve_t;

static void purge_expired_resolves(time_t now);
static void dns_found_answer(const char *address, uint8_t query_type,
                             int dns_answer,
                             const tor_addr_t *addr,
                             const char *hostname,
                             uint32_t ttl);
static void send_resolved_cell(edge_connection_t *conn, uint8_t answer_type,
                               const cached_resolve_t *resolve);
static int launch_resolve(cached_resolve_t *resolve);
static void add_wildcarded_test_address(const char *address);
static int configure_nameservers(int force);
static int answer_is_wildcarded(const char *ip);
static int dns_resolve_impl(edge_connection_t *exitconn, int is_resolve,
                            or_circuit_t *oncirc, char **resolved_to_hostname,
                            int *made_connection_pending_out,
                            cached_resolve_t **resolve_out);
static int set_exitconn_info_from_resolve(edge_connection_t *exitconn,
                                          const cached_resolve_t *resolve,
                                          char **hostname_out);
static int evdns_err_is_transient(int err);
static void inform_pending_connections(cached_resolve_t *resolve);
static void make_pending_resolve_cached(cached_resolve_t *cached);

#ifdef DEBUG_DNS_CACHE
static void assert_cache_ok_(void);
#define assert_cache_ok() assert_cache_ok_()
#else
#define assert_cache_ok() STMT_NIL
#endif
static void assert_resolve_ok(cached_resolve_t *resolve);

/** Hash table of cached_resolve objects. */
static HT_HEAD(cache_map, cached_resolve_t) cache_root;

/** Global: how many IPv6 requests have we made in all? */
static uint64_t n_ipv6_requests_made = 0;
/** Global: how many IPv6 requests have timed out? */
static uint64_t n_ipv6_timeouts = 0;
/** Global: Do we think that IPv6 DNS is broken? */
static int dns_is_broken_for_ipv6 = 0;

/** Function to compare hashed resolves on their addresses; used to
 * implement hash tables. */
static INLINE int
cached_resolves_eq(cached_resolve_t *a, cached_resolve_t *b)
{
  /* make this smarter one day? */
  assert_resolve_ok(a); // Not b; b may be just a search.
  return !strncmp(a->address, b->address, MAX_ADDRESSLEN);
}

/** Hash function for cached_resolve objects */
static INLINE unsigned int
cached_resolve_hash(cached_resolve_t *a)
{
  return (unsigned) siphash24g((const uint8_t*)a->address, strlen(a->address));
}

HT_PROTOTYPE(cache_map, cached_resolve_t, node, cached_resolve_hash,
             cached_resolves_eq)
HT_GENERATE(cache_map, cached_resolve_t, node, cached_resolve_hash,
            cached_resolves_eq, 0.6, malloc, realloc, free)

/** Initialize the DNS cache. */
static void
init_cache_map(void)
{
  HT_INIT(cache_map, &cache_root);
}

/** Helper: called by eventdns when eventdns wants to log something. */
static void
evdns_log_cb(int warn, const char *msg)
{
  const char *cp;
  static int all_down = 0;
  int severity = warn ? LOG_WARN : LOG_INFO;
  if (!strcmpstart(msg, "Resolve requested for") &&
      get_options()->SafeLogging) {
    log_info(LD_EXIT, "eventdns: Resolve requested.");
    return;
  } else if (!strcmpstart(msg, "Search: ")) {
    return;
  }
  if (!strcmpstart(msg, "Nameserver ") && (cp=strstr(msg, " has failed: "))) {
    char *ns = tor_strndup(msg+11, cp-(msg+11));
    const char *err = strchr(cp, ':')+2;
    tor_assert(err);
    /* Don't warn about a single failed nameserver; we'll warn with 'all
     * nameservers have failed' if we're completely out of nameservers;
     * otherwise, the situation is tolerable. */
    severity = LOG_INFO;
    control_event_server_status(LOG_NOTICE,
                                "NAMESERVER_STATUS NS=%s STATUS=DOWN ERR=%s",
                                ns, escaped(err));
    tor_free(ns);
  } else if (!strcmpstart(msg, "Nameserver ") &&
             (cp=strstr(msg, " is back up"))) {
    char *ns = tor_strndup(msg+11, cp-(msg+11));
    severity = (all_down && warn) ? LOG_NOTICE : LOG_INFO;
    all_down = 0;
    control_event_server_status(LOG_NOTICE,
                                "NAMESERVER_STATUS NS=%s STATUS=UP", ns);
    tor_free(ns);
  } else if (!strcmp(msg, "All nameservers have failed")) {
    control_event_server_status(LOG_WARN, "NAMESERVER_ALL_DOWN");
    all_down = 1;
  }
  tor_log(severity, LD_EXIT, "eventdns: %s", msg);
}

/** Helper: passed to eventdns.c as a callback so it can generate random
 * numbers for transaction IDs and 0x20-hack coding. */
static void
dns_randfn_(char *b, size_t n)
{
  crypto_rand(b,n);
}

/** Initialize the DNS subsystem; called by the OR process. */
int
dns_init(void)
{
  init_cache_map();
  evdns_set_random_bytes_fn(dns_randfn_);
  if (server_mode(get_options())) {
    int r = configure_nameservers(1);
    return r;
  }
  return 0;
}

/** Called when DNS-related options change (or may have changed).  Returns -1
 * on failure, 0 on success. */
int
dns_reset(void)
{
  const or_options_t *options = get_options();
  if (! server_mode(options)) {

    if (!the_evdns_base) {
      if (!(the_evdns_base = evdns_base_new(tor_libevent_get_base(), 0))) {
        log_err(LD_BUG, "Couldn't create an evdns_base");
        return -1;
      }
    }

    evdns_base_clear_nameservers_and_suspend(the_evdns_base);
    evdns_base_search_clear(the_evdns_base);
    nameservers_configured = 0;
    tor_free(resolv_conf_fname);
    resolv_conf_mtime = 0;
  } else {
    if (configure_nameservers(0) < 0) {
      return -1;
    }
  }
  return 0;
}

/** Return true iff the most recent attempt to initialize the DNS subsystem
 * failed. */
int
has_dns_init_failed(void)
{
  return nameserver_config_failed;
}

/** Helper: Given a TTL from a DNS response, determine what TTL to give the
 * OP that asked us to resolve it. */
uint32_t
dns_clip_ttl(uint32_t ttl)
{
  if (ttl < MIN_DNS_TTL)
    return MIN_DNS_TTL;
  else if (ttl > MAX_DNS_TTL)
    return MAX_DNS_TTL;
  else
    return ttl;
}

/** Helper: Given a TTL from a DNS response, determine how long to hold it in
 * our cache. */
static uint32_t
dns_get_expiry_ttl(uint32_t ttl)
{
  if (ttl < MIN_DNS_TTL)
    return MIN_DNS_TTL;
  else if (ttl > MAX_DNS_ENTRY_AGE)
    return MAX_DNS_ENTRY_AGE;
  else
    return ttl;
}

/** Helper: free storage held by an entry in the DNS cache. */
static void
free_cached_resolve_(cached_resolve_t *r)
{
  if (!r)
    return;
  while (r->pending_connections) {
    pending_connection_t *victim = r->pending_connections;
    r->pending_connections = victim->next;
    tor_free(victim);
  }
  if (r->res_status_hostname == RES_STATUS_DONE_OK)
    tor_free(r->result_ptr.hostname);
  r->magic = 0xFF00FF00;
  tor_free(r);
}

/** Compare two cached_resolve_t pointers by expiry time, and return
 * less-than-zero, zero, or greater-than-zero as appropriate. Used for
 * the priority queue implementation. */
static int
compare_cached_resolves_by_expiry_(const void *_a, const void *_b)
{
  const cached_resolve_t *a = _a, *b = _b;
  if (a->expire < b->expire)
    return -1;
  else if (a->expire == b->expire)
    return 0;
  else
    return 1;
}

/** Priority queue of cached_resolve_t objects to let us know when they
 * will expire. */
static smartlist_t *cached_resolve_pqueue = NULL;

static void
cached_resolve_add_answer(cached_resolve_t *resolve,
                          int query_type,
                          int dns_result,
                          const tor_addr_t *answer_addr,
                          const char *answer_hostname,
                          uint32_t ttl)
{
  if (query_type == DNS_PTR) {
    if (resolve->res_status_hostname != RES_STATUS_INFLIGHT)
      return;

    if (dns_result == DNS_ERR_NONE && answer_hostname) {
      resolve->result_ptr.hostname = tor_strdup(answer_hostname);
      resolve->res_status_hostname = RES_STATUS_DONE_OK;
    } else {
      resolve->result_ptr.err_hostname = dns_result;
      resolve->res_status_hostname = RES_STATUS_DONE_ERR;
    }
    resolve->ttl_hostname = ttl;
  } else if (query_type == DNS_IPv4_A) {
    if (resolve->res_status_ipv4 != RES_STATUS_INFLIGHT)
      return;

    if (dns_result == DNS_ERR_NONE && answer_addr &&
        tor_addr_family(answer_addr) == AF_INET) {
      resolve->result_ipv4.addr_ipv4 = tor_addr_to_ipv4h(answer_addr);
      resolve->res_status_ipv4 = RES_STATUS_DONE_OK;
    } else {
      resolve->result_ipv4.err_ipv4 = dns_result;
      resolve->res_status_ipv4 = RES_STATUS_DONE_ERR;
    }

  } else if (query_type == DNS_IPv6_AAAA) {
    if (resolve->res_status_ipv6 != RES_STATUS_INFLIGHT)
      return;

    if (dns_result == DNS_ERR_NONE && answer_addr &&
        tor_addr_family(answer_addr) == AF_INET6) {
      memcpy(&resolve->result_ipv6.addr_ipv6,
             tor_addr_to_in6(answer_addr),
             sizeof(struct in6_addr));
      resolve->res_status_ipv6 = RES_STATUS_DONE_OK;
    } else {
      resolve->result_ipv6.err_ipv6 = dns_result;
      resolve->res_status_ipv6 = RES_STATUS_DONE_ERR;
    }
  }
}

/** Return true iff there are no in-flight requests for <b>resolve</b>. */
static int
cached_resolve_have_all_answers(const cached_resolve_t *resolve)
{
  return (resolve->res_status_ipv4 != RES_STATUS_INFLIGHT &&
          resolve->res_status_ipv6 != RES_STATUS_INFLIGHT &&
          resolve->res_status_hostname != RES_STATUS_INFLIGHT);
}

/** Set an expiry time for a cached_resolve_t, and add it to the expiry
 * priority queue */
static void
set_expiry(cached_resolve_t *resolve, time_t expires)
{
  tor_assert(resolve && resolve->expire == 0);
  if (!cached_resolve_pqueue)
    cached_resolve_pqueue = smartlist_new();
  resolve->expire = expires;
  smartlist_pqueue_add(cached_resolve_pqueue,
                       compare_cached_resolves_by_expiry_,
                       STRUCT_OFFSET(cached_resolve_t, minheap_idx),
                       resolve);
}

/** Free all storage held in the DNS cache and related structures. */
void
dns_free_all(void)
{
  cached_resolve_t **ptr, **next, *item;
  assert_cache_ok();
  if (cached_resolve_pqueue) {
    SMARTLIST_FOREACH(cached_resolve_pqueue, cached_resolve_t *, res,
      {
        if (res->state == CACHE_STATE_DONE)
          free_cached_resolve_(res);
      });
  }
  for (ptr = HT_START(cache_map, &cache_root); ptr != NULL; ptr = next) {
    item = *ptr;
    next = HT_NEXT_RMV(cache_map, &cache_root, ptr);
    free_cached_resolve_(item);
  }
  HT_CLEAR(cache_map, &cache_root);
  smartlist_free(cached_resolve_pqueue);
  cached_resolve_pqueue = NULL;
  tor_free(resolv_conf_fname);
}

/** Remove every cached_resolve whose <b>expire</b> time is before or
 * equal to <b>now</b> from the cache. */
static void
purge_expired_resolves(time_t now)
{
  cached_resolve_t *resolve, *removed;
  pending_connection_t *pend;
  edge_connection_t *pendconn;

  assert_cache_ok();
  if (!cached_resolve_pqueue)
    return;

  while (smartlist_len(cached_resolve_pqueue)) {
    resolve = smartlist_get(cached_resolve_pqueue, 0);
    if (resolve->expire > now)
      break;
    smartlist_pqueue_pop(cached_resolve_pqueue,
                         compare_cached_resolves_by_expiry_,
                         STRUCT_OFFSET(cached_resolve_t, minheap_idx));

    if (resolve->state == CACHE_STATE_PENDING) {
      log_debug(LD_EXIT,
                "Expiring a dns resolve %s that's still pending. Forgot to "
                "cull it? DNS resolve didn't tell us about the timeout?",
                escaped_safe_str(resolve->address));
    } else if (resolve->state == CACHE_STATE_CACHED) {
      log_debug(LD_EXIT,
                "Forgetting old cached resolve (address %s, expires %lu)",
                escaped_safe_str(resolve->address),
                (unsigned long)resolve->expire);
      tor_assert(!resolve->pending_connections);
    } else {
      tor_assert(resolve->state == CACHE_STATE_DONE);
      tor_assert(!resolve->pending_connections);
    }

    if (resolve->pending_connections) {
      log_debug(LD_EXIT,
                "Closing pending connections on timed-out DNS resolve!");
      while (resolve->pending_connections) {
        pend = resolve->pending_connections;
        resolve->pending_connections = pend->next;
        /* Connections should only be pending if they have no socket. */
        tor_assert(!SOCKET_OK(pend->conn->base_.s));
        pendconn = pend->conn;
        /* Prevent double-remove */
        pendconn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
        if (!pendconn->base_.marked_for_close) {
          connection_edge_end(pendconn, END_STREAM_REASON_TIMEOUT);
          circuit_detach_stream(circuit_get_by_edge_conn(pendconn), pendconn);
          connection_free(TO_CONN(pendconn));
        }
        tor_free(pend);
      }
    }

    if (resolve->state == CACHE_STATE_CACHED ||
        resolve->state == CACHE_STATE_PENDING) {
      removed = HT_REMOVE(cache_map, &cache_root, resolve);
      if (removed != resolve) {
        log_err(LD_BUG, "The expired resolve we purged didn't match any in"
                " the cache. Tried to purge %s (%p); instead got %s (%p).",
                resolve->address, (void*)resolve,
                removed ? removed->address : "NULL", (void*)removed);
      }
      tor_assert(removed == resolve);
    } else {
      /* This should be in state DONE. Make sure it's not in the cache. */
      cached_resolve_t *tmp = HT_FIND(cache_map, &cache_root, resolve);
      tor_assert(tmp != resolve);
    }
    if (resolve->res_status_hostname == RES_STATUS_DONE_OK)
      tor_free(resolve->result_ptr.hostname);
    resolve->magic = 0xF0BBF0BB;
    tor_free(resolve);
  }

  assert_cache_ok();
}

/* argument for send_resolved_cell only, meaning "let the answer type be ipv4
 * or ipv6 depending on the connection's address". */
#define RESOLVED_TYPE_AUTO 0xff

/** Send a response to the RESOLVE request of a connection.
 * <b>answer_type</b> must be one of
 * RESOLVED_TYPE_(AUTO|ERROR|ERROR_TRANSIENT|).
 *
 * If <b>circ</b> is provided, and we have a cached answer, send the
 * answer back along circ; otherwise, send the answer back along
 * <b>conn</b>'s attached circuit.
 */
static void
send_resolved_cell(edge_connection_t *conn, uint8_t answer_type,
                   const cached_resolve_t *resolved)
{
  char buf[RELAY_PAYLOAD_SIZE], *cp = buf;
  size_t buflen = 0;
  uint32_t ttl;

  buf[0] = answer_type;
  ttl = dns_clip_ttl(conn->address_ttl);

  switch (answer_type)
    {
    case RESOLVED_TYPE_AUTO:
      if (resolved && resolved->res_status_ipv4 == RES_STATUS_DONE_OK) {
        cp[0] = RESOLVED_TYPE_IPV4;
        cp[1] = 4;
        set_uint32(cp+2, htonl(resolved->result_ipv4.addr_ipv4));
        set_uint32(cp+6, htonl(ttl));
        cp += 10;
      }
      if (resolved && resolved->res_status_ipv6 == RES_STATUS_DONE_OK) {
        const uint8_t *bytes = resolved->result_ipv6.addr_ipv6.s6_addr;
        cp[0] = RESOLVED_TYPE_IPV6;
        cp[1] = 16;
        memcpy(cp+2, bytes, 16);
        set_uint32(cp+18, htonl(ttl));
        cp += 22;
      }
      if (cp != buf) {
        buflen = cp - buf;
        break;
      } else {
        answer_type = RESOLVED_TYPE_ERROR;
        /* fall through. */
      }
    case RESOLVED_TYPE_ERROR_TRANSIENT:
    case RESOLVED_TYPE_ERROR:
      {
        const char *errmsg = "Error resolving hostname";
        size_t msglen = strlen(errmsg);

        buf[0] = answer_type;
        buf[1] = msglen;
        strlcpy(buf+2, errmsg, sizeof(buf)-2);
        set_uint32(buf+2+msglen, htonl(ttl));
        buflen = 6+msglen;
        break;
      }
    default:
      tor_assert(0);
      return;
    }
  // log_notice(LD_EXIT, "Sending a regular RESOLVED reply: ");

  connection_edge_send_command(conn, RELAY_COMMAND_RESOLVED, buf, buflen);
}

/** Send a response to the RESOLVE request of a connection for an in-addr.arpa
 * address on connection <b>conn</b> which yielded the result <b>hostname</b>.
 * The answer type will be RESOLVED_HOSTNAME.
 *
 * If <b>circ</b> is provided, and we have a cached answer, send the
 * answer back along circ; otherwise, send the answer back along
 * <b>conn</b>'s attached circuit.
 */
static void
send_resolved_hostname_cell(edge_connection_t *conn, const char *hostname)
{
  char buf[RELAY_PAYLOAD_SIZE];
  size_t buflen;
  uint32_t ttl;
  size_t namelen = strlen(hostname);
  tor_assert(hostname);

  tor_assert(namelen < 256);
  ttl = dns_clip_ttl(conn->address_ttl);

  buf[0] = RESOLVED_TYPE_HOSTNAME;
  buf[1] = (uint8_t)namelen;
  memcpy(buf+2, hostname, namelen);
  set_uint32(buf+2+namelen, htonl(ttl));
  buflen = 2+namelen+4;

  // log_notice(LD_EXIT, "Sending a reply RESOLVED reply: %s", hostname);
  connection_edge_send_command(conn, RELAY_COMMAND_RESOLVED, buf, buflen);
  // log_notice(LD_EXIT, "Sent");
}

/** See if we have a cache entry for <b>exitconn</b>-\>address. If so,
 * if resolve valid, put it into <b>exitconn</b>-\>addr and return 1.
 * If resolve failed, free exitconn and return -1.
 *
 * (For EXIT_PURPOSE_RESOLVE connections, send back a RESOLVED error cell
 * on returning -1.  For EXIT_PURPOSE_CONNECT connections, there's no
 * need to send back an END cell, since connection_exit_begin_conn will
 * do that for us.)
 *
 * If we have a cached answer, send the answer back along <b>exitconn</b>'s
 * circuit.
 *
 * Else, if seen before and pending, add conn to the pending list,
 * and return 0.
 *
 * Else, if not seen before, add conn to pending list, hand to
 * dns farm, and return 0.
 *
 * Exitconn's on_circuit field must be set, but exitconn should not
 * yet be linked onto the n_streams/resolving_streams list of that circuit.
 * On success, link the connection to n_streams if it's an exit connection.
 * On "pending", link the connection to resolving streams.  Otherwise,
 * clear its on_circuit field.
 */
int
dns_resolve(edge_connection_t *exitconn)
{
  or_circuit_t *oncirc = TO_OR_CIRCUIT(exitconn->on_circuit);
  int is_resolve, r;
  int made_connection_pending = 0;
  char *hostname = NULL;
  cached_resolve_t *resolve = NULL;
  is_resolve = exitconn->base_.purpose == EXIT_PURPOSE_RESOLVE;

  r = dns_resolve_impl(exitconn, is_resolve, oncirc, &hostname,
                       &made_connection_pending, &resolve);

  switch (r) {
    case 1:
      /* We got an answer without a lookup -- either the answer was
       * cached, or it was obvious (like an IP address). */
      if (is_resolve) {
        /* Send the answer back right now, and detach. */
        if (hostname)
          send_resolved_hostname_cell(exitconn, hostname);
        else
          send_resolved_cell(exitconn, RESOLVED_TYPE_AUTO, resolve);
        exitconn->on_circuit = NULL;
      } else {
        /* Add to the n_streams list; the calling function will send back a
         * connected cell. */
        exitconn->next_stream = oncirc->n_streams;
        oncirc->n_streams = exitconn;
      }
      break;
    case 0:
      /* The request is pending: add the connection into the linked list of
       * resolving_streams on this circuit. */
      exitconn->base_.state = EXIT_CONN_STATE_RESOLVING;
      exitconn->next_stream = oncirc->resolving_streams;
      oncirc->resolving_streams = exitconn;
      break;
    case -2:
    case -1:
      /* The request failed before it could start: cancel this connection,
       * and stop everybody waiting for the same connection. */
      if (is_resolve) {
        send_resolved_cell(exitconn,
             (r == -1) ? RESOLVED_TYPE_ERROR : RESOLVED_TYPE_ERROR_TRANSIENT,
             NULL);
      }

      exitconn->on_circuit = NULL;

      dns_cancel_pending_resolve(exitconn->base_.address);

      if (!made_connection_pending && !exitconn->base_.marked_for_close) {
        /* If we made the connection pending, then we freed it already in
         * dns_cancel_pending_resolve().  If we marked it for close, it'll
         * get freed from the main loop.  Otherwise, can free it now. */
        connection_free(TO_CONN(exitconn));
      }
      break;
    default:
      tor_assert(0);
  }

  tor_free(hostname);
  return r;
}

/** Helper function for dns_resolve: same functionality, but does not handle:
 *     - marking connections on error and clearing their on_circuit
 *     - linking connections to n_streams/resolving_streams,
 *     - sending resolved cells if we have an answer/error right away,
 *
 * Return -2 on a transient error. If it's a reverse resolve and it's
 * successful, sets *<b>hostname_out</b> to a newly allocated string
 * holding the cached reverse DNS value.
 *
 * Set *<b>made_connection_pending_out</b> to true if we have placed
 * <b>exitconn</b> on the list of pending connections for some resolve; set it
 * to false otherwise.
 *
 * Set *<b>resolve_out</b> to a cached resolve, if we found one.
 */
static int
dns_resolve_impl(edge_connection_t *exitconn, int is_resolve,
                 or_circuit_t *oncirc, char **hostname_out,
                 int *made_connection_pending_out,
                 cached_resolve_t **resolve_out)
{
  cached_resolve_t *resolve;
  cached_resolve_t search;
  pending_connection_t *pending_connection;
  int is_reverse = 0;
  tor_addr_t addr;
  time_t now = time(NULL);
  int r;
  assert_connection_ok(TO_CONN(exitconn), 0);
  tor_assert(!SOCKET_OK(exitconn->base_.s));
  assert_cache_ok();
  tor_assert(oncirc);
  *made_connection_pending_out = 0;

  /* first check if exitconn->base_.address is an IP. If so, we already
   * know the answer. */
  if (tor_addr_parse(&addr, exitconn->base_.address) >= 0) {
    if (tor_addr_family(&addr) == AF_INET ||
        tor_addr_family(&addr) == AF_INET6) {
      tor_addr_copy(&exitconn->base_.addr, &addr);
      exitconn->address_ttl = DEFAULT_DNS_TTL;
      return 1;
    } else {
      /* XXXX unspec? Bogus? */
      return -1;
    }
  }

  /* If we're a non-exit, don't even do DNS lookups. */
  if (router_my_exit_policy_is_reject_star())
    return -1;

  if (address_is_invalid_destination(exitconn->base_.address, 0)) {
    tor_log(LOG_PROTOCOL_WARN, LD_EXIT,
        "Rejecting invalid destination address %s",
        escaped_safe_str(exitconn->base_.address));
    return -1;
  }

  /* then take this opportunity to see if there are any expired
   * resolves in the hash table. */
  purge_expired_resolves(now);

  /* lower-case exitconn->base_.address, so it's in canonical form */
  tor_strlower(exitconn->base_.address);

  /* Check whether this is a reverse lookup.  If it's malformed, or it's a
   * .in-addr.arpa address but this isn't a resolve request, kill the
   * connection.
   */
  if ((r = tor_addr_parse_PTR_name(&addr, exitconn->base_.address,
                                              AF_UNSPEC, 0)) != 0) {
    if (r == 1) {
      is_reverse = 1;
      if (tor_addr_is_internal(&addr, 0)) /* internal address? */
        return -1;
    }

    if (!is_reverse || !is_resolve) {
      if (!is_reverse)
        log_info(LD_EXIT, "Bad .in-addr.arpa address \"%s\"; sending error.",
                 escaped_safe_str(exitconn->base_.address));
      else if (!is_resolve)
        log_info(LD_EXIT,
                 "Attempt to connect to a .in-addr.arpa address \"%s\"; "
                 "sending error.",
                 escaped_safe_str(exitconn->base_.address));

      return -1;
    }
    //log_notice(LD_EXIT, "Looks like an address %s",
    //exitconn->base_.address);
  }
  exitconn->is_reverse_dns_lookup = is_reverse;

  /* now check the hash table to see if 'address' is already there. */
  strlcpy(search.address, exitconn->base_.address, sizeof(search.address));
  resolve = HT_FIND(cache_map, &cache_root, &search);
  if (resolve && resolve->expire > now) { /* already there */
    switch (resolve->state) {
      case CACHE_STATE_PENDING:
        /* add us to the pending list */
        pending_connection = tor_malloc_zero(
                                      sizeof(pending_connection_t));
        pending_connection->conn = exitconn;
        pending_connection->next = resolve->pending_connections;
        resolve->pending_connections = pending_connection;
        *made_connection_pending_out = 1;
        log_debug(LD_EXIT,"Connection (fd "TOR_SOCKET_T_FORMAT") waiting "
                  "for pending DNS resolve of %s", exitconn->base_.s,
                  escaped_safe_str(exitconn->base_.address));
        return 0;
      case CACHE_STATE_CACHED:
        log_debug(LD_EXIT,"Connection (fd "TOR_SOCKET_T_FORMAT") found "
                  "cached answer for %s",
                  exitconn->base_.s,
                  escaped_safe_str(resolve->address));

        *resolve_out = resolve;

        return set_exitconn_info_from_resolve(exitconn, resolve, hostname_out);
      case CACHE_STATE_DONE:
        log_err(LD_BUG, "Found a 'DONE' dns resolve still in the cache.");
        tor_fragile_assert();
    }
    tor_assert(0);
  }
  tor_assert(!resolve);
  /* not there, need to add it */
  resolve = tor_malloc_zero(sizeof(cached_resolve_t));
  resolve->magic = CACHED_RESOLVE_MAGIC;
  resolve->state = CACHE_STATE_PENDING;
  resolve->minheap_idx = -1;
  strlcpy(resolve->address, exitconn->base_.address, sizeof(resolve->address));

  /* add this connection to the pending list */
  pending_connection = tor_malloc_zero(sizeof(pending_connection_t));
  pending_connection->conn = exitconn;
  resolve->pending_connections = pending_connection;
  *made_connection_pending_out = 1;

  /* Add this resolve to the cache and priority queue. */
  HT_INSERT(cache_map, &cache_root, resolve);
  set_expiry(resolve, now + RESOLVE_MAX_TIMEOUT);

  log_debug(LD_EXIT,"Launching %s.",
            escaped_safe_str(exitconn->base_.address));
  assert_cache_ok();

  return launch_resolve(resolve);
}

/** Given an exit connection <b>exitconn</b>, and a cached_resolve_t
 * <b>resolve</b> whose DNS lookups have all succeeded or failed, update the
 * appropriate fields (address_ttl and addr) of <b>exitconn</b>.
 *
 * If this is a reverse lookup, set *<b>hostname_out</b> to a newly allocated
 * copy of the name resulting hostname.
 *
 * Return -2 on a transient error, -1 on a permenent error, and 1 on
 * a successful lookup.
 */
static int
set_exitconn_info_from_resolve(edge_connection_t *exitconn,
                               const cached_resolve_t *resolve,
                               char **hostname_out)
{
  int ipv4_ok, ipv6_ok, answer_with_ipv4, r;
  uint32_t begincell_flags;
  const int is_resolve = exitconn->base_.purpose == EXIT_PURPOSE_RESOLVE;
  tor_assert(exitconn);
  tor_assert(resolve);

  if (exitconn->is_reverse_dns_lookup) {
    exitconn->address_ttl = resolve->ttl_hostname;
    if (resolve->res_status_hostname == RES_STATUS_DONE_OK) {
      *hostname_out = tor_strdup(resolve->result_ptr.hostname);
      return 1;
    } else {
      return -1;
    }
  }

  /* If we're here then the connection wants one or either of ipv4, ipv6, and
   * we can give it one or both. */
  if (is_resolve) {
    begincell_flags = BEGIN_FLAG_IPV6_OK;
  } else {
    begincell_flags = exitconn->begincell_flags;
  }

  ipv4_ok = (resolve->res_status_ipv4 == RES_STATUS_DONE_OK) &&
    ! (begincell_flags & BEGIN_FLAG_IPV4_NOT_OK);
  ipv6_ok = (resolve->res_status_ipv6 == RES_STATUS_DONE_OK) &&
    (begincell_flags & BEGIN_FLAG_IPV6_OK) &&
    get_options()->IPv6Exit;

  /* Now decide which one to actually give. */
  if (ipv4_ok && ipv6_ok && is_resolve) {
    answer_with_ipv4 = 1;
  } else if (ipv4_ok && ipv6_ok) {
    /* If we have both, see if our exit policy has an opinion. */
    const uint16_t port = exitconn->base_.port;
    int ipv4_allowed, ipv6_allowed;
    tor_addr_t a4, a6;
    tor_addr_from_ipv4h(&a4, resolve->result_ipv4.addr_ipv4);
    tor_addr_from_in6(&a6, &resolve->result_ipv6.addr_ipv6);
    ipv4_allowed = !router_compare_to_my_exit_policy(&a4, port);
    ipv6_allowed = !router_compare_to_my_exit_policy(&a6, port);
    if (ipv4_allowed && !ipv6_allowed) {
      answer_with_ipv4 = 1;
    } else if (ipv6_allowed && !ipv4_allowed) {
      answer_with_ipv4 = 0;
    } else {
      /* Our exit policy would permit both.  Answer with whichever the user
       * prefers */
      answer_with_ipv4 = !(begincell_flags &
                           BEGIN_FLAG_IPV6_PREFERRED);
    }
  } else {
    /* Otherwise if one is okay, send it back. */
    if (ipv4_ok) {
      answer_with_ipv4 = 1;
    } else if (ipv6_ok) {
      answer_with_ipv4 = 0;
    } else {
      /* Neither one was okay. Choose based on user preference. */
      answer_with_ipv4 = !(begincell_flags &
                           BEGIN_FLAG_IPV6_PREFERRED);
    }
  }

  /* Finally, we write the answer back. */
  r = 1;
  if (answer_with_ipv4) {
    if (resolve->res_status_ipv4 == RES_STATUS_DONE_OK) {
      tor_addr_from_ipv4h(&exitconn->base_.addr,
                          resolve->result_ipv4.addr_ipv4);
    } else {
      r = evdns_err_is_transient(resolve->result_ipv4.err_ipv4) ? -2 : -1;
    }

    exitconn->address_ttl = resolve->ttl_ipv4;
  } else {
    if (resolve->res_status_ipv6 == RES_STATUS_DONE_OK) {
      tor_addr_from_in6(&exitconn->base_.addr,
                        &resolve->result_ipv6.addr_ipv6);
    } else {
      r = evdns_err_is_transient(resolve->result_ipv6.err_ipv6) ? -2 : -1;
    }

    exitconn->address_ttl = resolve->ttl_ipv6;
  }

  return r;
}

/** Log an error and abort if conn is waiting for a DNS resolve.
 */
void
assert_connection_edge_not_dns_pending(edge_connection_t *conn)
{
  pending_connection_t *pend;
  cached_resolve_t search;

#if 1
  cached_resolve_t *resolve;
  strlcpy(search.address, conn->base_.address, sizeof(search.address));
  resolve = HT_FIND(cache_map, &cache_root, &search);
  if (!resolve)
    return;
  for (pend = resolve->pending_connections; pend; pend = pend->next) {
    tor_assert(pend->conn != conn);
  }
#else
  cached_resolve_t **resolve;
  HT_FOREACH(resolve, cache_map, &cache_root) {
    for (pend = (*resolve)->pending_connections; pend; pend = pend->next) {
      tor_assert(pend->conn != conn);
    }
  }
#endif
}

/** Log an error and abort if any connection waiting for a DNS resolve is
 * corrupted. */
void
assert_all_pending_dns_resolves_ok(void)
{
  pending_connection_t *pend;
  cached_resolve_t **resolve;

  HT_FOREACH(resolve, cache_map, &cache_root) {
    for (pend = (*resolve)->pending_connections;
         pend;
         pend = pend->next) {
      assert_connection_ok(TO_CONN(pend->conn), 0);
      tor_assert(!SOCKET_OK(pend->conn->base_.s));
      tor_assert(!connection_in_array(TO_CONN(pend->conn)));
    }
  }
}

/** Remove <b>conn</b> from the list of connections waiting for conn-\>address.
 */
void
connection_dns_remove(edge_connection_t *conn)
{
  pending_connection_t *pend, *victim;
  cached_resolve_t search;
  cached_resolve_t *resolve;

  tor_assert(conn->base_.type == CONN_TYPE_EXIT);
  tor_assert(conn->base_.state == EXIT_CONN_STATE_RESOLVING);

  strlcpy(search.address, conn->base_.address, sizeof(search.address));

  resolve = HT_FIND(cache_map, &cache_root, &search);
  if (!resolve) {
    log_notice(LD_BUG, "Address %s is not pending. Dropping.",
               escaped_safe_str(conn->base_.address));
    return;
  }

  tor_assert(resolve->pending_connections);
  assert_connection_ok(TO_CONN(conn),0);

  pend = resolve->pending_connections;

  if (pend->conn == conn) {
    resolve->pending_connections = pend->next;
    tor_free(pend);
    log_debug(LD_EXIT, "First connection (fd "TOR_SOCKET_T_FORMAT") no "
              "longer waiting for resolve of %s",
              conn->base_.s,
              escaped_safe_str(conn->base_.address));
    return;
  } else {
    for ( ; pend->next; pend = pend->next) {
      if (pend->next->conn == conn) {
        victim = pend->next;
        pend->next = victim->next;
        tor_free(victim);
        log_debug(LD_EXIT,
                  "Connection (fd "TOR_SOCKET_T_FORMAT") no longer waiting "
                  "for resolve of %s",
                  conn->base_.s, escaped_safe_str(conn->base_.address));
        return; /* more are pending */
      }
    }
    log_warn(LD_BUG, "Connection (fd "TOR_SOCKET_T_FORMAT") was not waiting "
             "for a resolve of %s, but we tried to remove it.",
             conn->base_.s, escaped_safe_str(conn->base_.address));
  }
}

/** Mark all connections waiting for <b>address</b> for close.  Then cancel
 * the resolve for <b>address</b> itself, and remove any cached results for
 * <b>address</b> from the cache.
 */
void
dns_cancel_pending_resolve(const char *address)
{
  pending_connection_t *pend;
  cached_resolve_t search;
  cached_resolve_t *resolve, *tmp;
  edge_connection_t *pendconn;
  circuit_t *circ;

  strlcpy(search.address, address, sizeof(search.address));

  resolve = HT_FIND(cache_map, &cache_root, &search);
  if (!resolve)
    return;

  if (resolve->state != CACHE_STATE_PENDING) {
    /* We can get into this state if we never actually created the pending
     * resolve, due to finding an earlier cached error or something.  Just
     * ignore it. */
    if (resolve->pending_connections) {
      log_warn(LD_BUG,
               "Address %s is not pending but has pending connections!",
               escaped_safe_str(address));
      tor_fragile_assert();
    }
    return;
  }

  if (!resolve->pending_connections) {
    log_warn(LD_BUG,
             "Address %s is pending but has no pending connections!",
             escaped_safe_str(address));
    tor_fragile_assert();
    return;
  }
  tor_assert(resolve->pending_connections);

  /* mark all pending connections to fail */
  log_debug(LD_EXIT,
             "Failing all connections waiting on DNS resolve of %s",
             escaped_safe_str(address));
  while (resolve->pending_connections) {
    pend = resolve->pending_connections;
    pend->conn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
    pendconn = pend->conn;
    assert_connection_ok(TO_CONN(pendconn), 0);
    tor_assert(!SOCKET_OK(pendconn->base_.s));
    if (!pendconn->base_.marked_for_close) {
      connection_edge_end(pendconn, END_STREAM_REASON_RESOLVEFAILED);
    }
    circ = circuit_get_by_edge_conn(pendconn);
    if (circ)
      circuit_detach_stream(circ, pendconn);
    if (!pendconn->base_.marked_for_close)
      connection_free(TO_CONN(pendconn));
    resolve->pending_connections = pend->next;
    tor_free(pend);
  }

  tmp = HT_REMOVE(cache_map, &cache_root, resolve);
  if (tmp != resolve) {
    log_err(LD_BUG, "The cancelled resolve we purged didn't match any in"
            " the cache. Tried to purge %s (%p); instead got %s (%p).",
            resolve->address, (void*)resolve,
            tmp ? tmp->address : "NULL", (void*)tmp);
  }
  tor_assert(tmp == resolve);

  resolve->state = CACHE_STATE_DONE;
}

/** Return true iff <b>address</b> is one of the addresses we use to verify
 * that well-known sites aren't being hijacked by our DNS servers. */
static INLINE int
is_test_address(const char *address)
{
  const or_options_t *options = get_options();
  return options->ServerDNSTestAddresses &&
    smartlist_contains_string_case(options->ServerDNSTestAddresses, address);
}

/** Called on the OR side when the eventdns library tells us the outcome of a
 * single DNS resolve: remember the answer, and tell all pending connections
 * about the result of the lookup if the lookup is now done.  (<b>address</b>
 * is a NUL-terminated string containing the address to look up;
 * <b>query_type</b> is one of DNS_{IPv4_A,IPv6_AAAA,PTR}; <b>dns_answer</b>
 * is DNS_OK or one of DNS_ERR_*, <b>addr</b> is an IPv4 or IPv6 address if we
 * got one; <b>hostname</b> is a hostname fora PTR request if we got one, and
 * <b>ttl</b> is the time-to-live of this answer, in seconds.)
 */
static void
dns_found_answer(const char *address, uint8_t query_type,
                 int dns_answer,
                 const tor_addr_t *addr,
                 const char *hostname, uint32_t ttl)
{
  cached_resolve_t search;
  cached_resolve_t *resolve;

  assert_cache_ok();

  strlcpy(search.address, address, sizeof(search.address));

  resolve = HT_FIND(cache_map, &cache_root, &search);
  if (!resolve) {
    int is_test_addr = is_test_address(address);
    if (!is_test_addr)
      log_info(LD_EXIT,"Resolved unasked address %s; ignoring.",
               escaped_safe_str(address));
    return;
  }
  assert_resolve_ok(resolve);

  if (resolve->state != CACHE_STATE_PENDING) {
    /* XXXX Maybe update addr? or check addr for consistency? Or let
     * VALID replace FAILED? */
    int is_test_addr = is_test_address(address);
    if (!is_test_addr)
      log_notice(LD_EXIT,
                 "Resolved %s which was already resolved; ignoring",
                 escaped_safe_str(address));
    tor_assert(resolve->pending_connections == NULL);
    return;
  }

  cached_resolve_add_answer(resolve, query_type, dns_answer,
                            addr, hostname, ttl);

  if (cached_resolve_have_all_answers(resolve)) {
    inform_pending_connections(resolve);

    make_pending_resolve_cached(resolve);
  }
}

/** Given a pending cached_resolve_t that we just finished resolving,
 * inform every connection that was waiting for the outcome of that
 * resolution. */
static void
inform_pending_connections(cached_resolve_t *resolve)
{
  pending_connection_t *pend;
  edge_connection_t *pendconn;
  int r;

  while (resolve->pending_connections) {
    char *hostname = NULL;
    pend = resolve->pending_connections;
    pendconn = pend->conn; /* don't pass complex things to the
                              connection_mark_for_close macro */
    assert_connection_ok(TO_CONN(pendconn),time(NULL));

    if (pendconn->base_.marked_for_close) {
      /* prevent double-remove. */
      pendconn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
      resolve->pending_connections = pend->next;
      tor_free(pend);
      continue;
    }

    r = set_exitconn_info_from_resolve(pendconn,
                                       resolve,
                                       &hostname);

    if (r < 0) {
      /* prevent double-remove. */
      pendconn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
      if (pendconn->base_.purpose == EXIT_PURPOSE_CONNECT) {
        connection_edge_end(pendconn, END_STREAM_REASON_RESOLVEFAILED);
        /* This detach must happen after we send the end cell. */
        circuit_detach_stream(circuit_get_by_edge_conn(pendconn), pendconn);
      } else {
        send_resolved_cell(pendconn, r == -1 ?
                         RESOLVED_TYPE_ERROR : RESOLVED_TYPE_ERROR_TRANSIENT,
                         NULL);
        /* This detach must happen after we send the resolved cell. */
        circuit_detach_stream(circuit_get_by_edge_conn(pendconn), pendconn);
      }
      connection_free(TO_CONN(pendconn));
    } else {
      circuit_t *circ;
      if (pendconn->base_.purpose == EXIT_PURPOSE_CONNECT) {
        /* prevent double-remove. */
        pend->conn->base_.state = EXIT_CONN_STATE_CONNECTING;

        circ = circuit_get_by_edge_conn(pend->conn);
        tor_assert(circ);
        tor_assert(!CIRCUIT_IS_ORIGIN(circ));
        /* unlink pend->conn from resolving_streams, */
        circuit_detach_stream(circ, pend->conn);
        /* and link it to n_streams */
        pend->conn->next_stream = TO_OR_CIRCUIT(circ)->n_streams;
        pend->conn->on_circuit = circ;
        TO_OR_CIRCUIT(circ)->n_streams = pend->conn;

        connection_exit_connect(pend->conn);
      } else {
        /* prevent double-remove.  This isn't really an accurate state,
         * but it does the right thing. */
        pendconn->base_.state = EXIT_CONN_STATE_RESOLVEFAILED;
        if (pendconn->is_reverse_dns_lookup)
          send_resolved_hostname_cell(pendconn, hostname);
        else
          send_resolved_cell(pendconn, RESOLVED_TYPE_AUTO, resolve);
        circ = circuit_get_by_edge_conn(pendconn);
        tor_assert(circ);
        circuit_detach_stream(circ, pendconn);
        connection_free(TO_CONN(pendconn));
      }
    }
    resolve->pending_connections = pend->next;
    tor_free(pend);
    tor_free(hostname);
  }
}

/** Remove a pending cached_resolve_t from the hashtable, and add a
 * corresponding cached cached_resolve_t.
 *
 * This function is only necessary because of the perversity of our
 * cache timeout code; see inline comment for ideas on eliminating it.
 **/
static void
make_pending_resolve_cached(cached_resolve_t *resolve)
{
  cached_resolve_t *removed;

  resolve->state = CACHE_STATE_DONE;
  removed = HT_REMOVE(cache_map, &cache_root, resolve);
  if (removed != resolve) {
    log_err(LD_BUG, "The pending resolve we found wasn't removable from"
            " the cache. Tried to purge %s (%p); instead got %s (%p).",
            resolve->address, (void*)resolve,
            removed ? removed->address : "NULL", (void*)removed);
  }
  assert_resolve_ok(resolve);
  assert_cache_ok();
  /* The resolve will eventually just hit the time-out in the expiry queue and
  * expire. See fd0bafb0dedc7e2 for a brief explanation of how this got that
  * way.  XXXXX we could do better!*/

  {
    cached_resolve_t *new_resolve = tor_memdup(resolve,
                                               sizeof(cached_resolve_t));
    uint32_t ttl = UINT32_MAX;
    new_resolve->expire = 0; /* So that set_expiry won't croak. */
    if (resolve->res_status_hostname == RES_STATUS_DONE_OK)
      new_resolve->result_ptr.hostname =
        tor_strdup(resolve->result_ptr.hostname);

    new_resolve->state = CACHE_STATE_CACHED;

    assert_resolve_ok(new_resolve);
    HT_INSERT(cache_map, &cache_root, new_resolve);

    if ((resolve->res_status_ipv4 == RES_STATUS_DONE_OK ||
         resolve->res_status_ipv4 == RES_STATUS_DONE_ERR) &&
        resolve->ttl_ipv4 < ttl)
      ttl = resolve->ttl_ipv4;

    if ((resolve->res_status_ipv6 == RES_STATUS_DONE_OK ||
         resolve->res_status_ipv6 == RES_STATUS_DONE_ERR) &&
        resolve->ttl_ipv6 < ttl)
      ttl = resolve->ttl_ipv6;

    if ((resolve->res_status_hostname == RES_STATUS_DONE_OK ||
         resolve->res_status_hostname == RES_STATUS_DONE_ERR) &&
        resolve->ttl_hostname < ttl)
      ttl = resolve->ttl_hostname;

    set_expiry(new_resolve, time(NULL) + dns_get_expiry_ttl(ttl));
  }

  assert_cache_ok();
}

/** Eventdns helper: return true iff the eventdns result <b>err</b> is
 * a transient failure. */
static int
evdns_err_is_transient(int err)
{
  switch (err)
  {
    case DNS_ERR_SERVERFAILED:
    case DNS_ERR_TRUNCATED:
    case DNS_ERR_TIMEOUT:
      return 1;
    default:
      return 0;
  }
}

/** Configure eventdns nameservers if force is true, or if the configuration
 * has changed since the last time we called this function, or if we failed on
 * our last attempt.  On Unix, this reads from /etc/resolv.conf or
 * options->ServerDNSResolvConfFile; on Windows, this reads from
 * options->ServerDNSResolvConfFile or the registry.  Return 0 on success or
 * -1 on failure. */
static int
configure_nameservers(int force)
{
  const or_options_t *options;
  const char *conf_fname;
  struct stat st;
  int r, flags;
  options = get_options();
  conf_fname = options->ServerDNSResolvConfFile;
#ifndef _WIN32
  if (!conf_fname)
    conf_fname = "/etc/resolv.conf";
#endif
  flags = DNS_OPTIONS_ALL;

  if (!the_evdns_base) {
    if (!(the_evdns_base = evdns_base_new(tor_libevent_get_base(), 0))) {
      log_err(LD_BUG, "Couldn't create an evdns_base");
      return -1;
    }
  }

#ifdef HAVE_EVDNS_SET_DEFAULT_OUTGOING_BIND_ADDRESS
  if (! tor_addr_is_null(&options->OutboundBindAddressIPv4_)) {
    int socklen;
    struct sockaddr_storage ss;
    socklen = tor_addr_to_sockaddr(&options->OutboundBindAddressIPv4_, 0,
                                   (struct sockaddr *)&ss, sizeof(ss));
    if (socklen <= 0) {
      log_warn(LD_BUG, "Couldn't convert outbound bind address to sockaddr."
               " Ignoring.");
    } else {
      evdns_base_set_default_outgoing_bind_address(the_evdns_base,
                                                   (struct sockaddr *)&ss,
                                                   socklen);
    }
  }
#endif

  evdns_set_log_fn(evdns_log_cb);
  if (conf_fname) {
    log_debug(LD_FS, "stat()ing %s", conf_fname);
    if (stat(sandbox_intern_string(conf_fname), &st)) {
      log_warn(LD_EXIT, "Unable to stat resolver configuration in '%s': %s",
               conf_fname, strerror(errno));
      goto err;
    }
    if (!force && resolv_conf_fname && !strcmp(conf_fname,resolv_conf_fname)
        && st.st_mtime == resolv_conf_mtime) {
      log_info(LD_EXIT, "No change to '%s'", conf_fname);
      return 0;
    }
    if (nameservers_configured) {
      evdns_base_search_clear(the_evdns_base);
      evdns_base_clear_nameservers_and_suspend(the_evdns_base);
    }
#if defined(DNS_OPTION_HOSTSFILE) && defined(USE_LIBSECCOMP)
    if (flags & DNS_OPTION_HOSTSFILE) {
      flags ^= DNS_OPTION_HOSTSFILE;
      log_debug(LD_FS, "Loading /etc/hosts");
      evdns_base_load_hosts(the_evdns_base,
          sandbox_intern_string("/etc/hosts"));
    }
#endif
    log_info(LD_EXIT, "Parsing resolver configuration in '%s'", conf_fname);
    if ((r = evdns_base_resolv_conf_parse(the_evdns_base, flags,
        sandbox_intern_string(conf_fname)))) {
      log_warn(LD_EXIT, "Unable to parse '%s', or no nameservers in '%s' (%d)",
               conf_fname, conf_fname, r);
      goto err;
    }
    if (evdns_base_count_nameservers(the_evdns_base) == 0) {
      log_warn(LD_EXIT, "Unable to find any nameservers in '%s'.", conf_fname);
      goto err;
    }
    tor_free(resolv_conf_fname);
    resolv_conf_fname = tor_strdup(conf_fname);
    resolv_conf_mtime = st.st_mtime;
    if (nameservers_configured)
      evdns_base_resume(the_evdns_base);
  }
#ifdef _WIN32
  else {
    if (nameservers_configured) {
      evdns_base_search_clear(the_evdns_base);
      evdns_base_clear_nameservers_and_suspend(the_evdns_base);
    }
    if (evdns_base_config_windows_nameservers(the_evdns_base))  {
      log_warn(LD_EXIT,"Could not config nameservers.");
      goto err;
    }
    if (evdns_base_count_nameservers(the_evdns_base) == 0) {
      log_warn(LD_EXIT, "Unable to find any platform nameservers in "
               "your Windows configuration.");
      goto err;
    }
    if (nameservers_configured)
      evdns_base_resume(the_evdns_base);
    tor_free(resolv_conf_fname);
    resolv_conf_mtime = 0;
  }
#endif

#define SET(k,v)  evdns_base_set_option_(the_evdns_base, (k), (v))

  if (evdns_base_count_nameservers(the_evdns_base) == 1) {
    SET("max-timeouts:", "16");
    SET("timeout:", "10");
  } else {
    SET("max-timeouts:", "3");
    SET("timeout:", "5");
  }

  if (options->ServerDNSRandomizeCase)
    SET("randomize-case:", "1");
  else
    SET("randomize-case:", "0");

#undef SET

  dns_servers_relaunch_checks();

  nameservers_configured = 1;
  if (nameserver_config_failed) {
    nameserver_config_failed = 0;
    /* XXX the three calls to republish the descriptor might be producing
     * descriptors that are only cosmetically different, especially on
     * non-exit relays! -RD */
    mark_my_descriptor_dirty("dns resolvers back");
  }
  return 0;
 err:
  nameservers_configured = 0;
  if (! nameserver_config_failed) {
    nameserver_config_failed = 1;
    mark_my_descriptor_dirty("dns resolvers failed");
  }
  return -1;
}

/** For eventdns: Called when we get an answer for a request we launched.
 * See eventdns.h for arguments; 'arg' holds the address we tried to resolve.
 */
static void
evdns_callback(int result, char type, int count, int ttl, void *addresses,
               void *arg)
{
  char *arg_ = arg;
  uint8_t orig_query_type = arg_[0];
  char *string_address = arg_ + 1;
  tor_addr_t addr;
  const char *hostname = NULL;
  int was_wildcarded = 0;

  tor_addr_make_unspec(&addr);

  /* Keep track of whether IPv6 is working */
  if (type == DNS_IPv6_AAAA) {
    if (result == DNS_ERR_TIMEOUT) {
      ++n_ipv6_timeouts;
    }

    if (n_ipv6_timeouts > 10 &&
        n_ipv6_timeouts > n_ipv6_requests_made / 2) {
      if (! dns_is_broken_for_ipv6) {
        log_notice(LD_EXIT, "More than half of our IPv6 requests seem to "
                   "have timed out. I'm going to assume I can't get AAAA "
                   "responses.");
        dns_is_broken_for_ipv6 = 1;
      }
    }
  }

  if (result == DNS_ERR_NONE) {
    if (type == DNS_IPv4_A && count) {
      char answer_buf[INET_NTOA_BUF_LEN+1];
      char *escaped_address;
      uint32_t *addrs = addresses;
      tor_addr_from_ipv4n(&addr, addrs[0]);

      tor_addr_to_str(answer_buf, &addr, sizeof(answer_buf), 0);
      escaped_address = esc_for_log(string_address);

      if (answer_is_wildcarded(answer_buf)) {
        log_debug(LD_EXIT, "eventdns said that %s resolves to ISP-hijacked "
                  "address %s; treating as a failure.",
                  safe_str(escaped_address),
                  escaped_safe_str(answer_buf));
        was_wildcarded = 1;
        tor_addr_make_unspec(&addr);
        result = DNS_ERR_NOTEXIST;
      } else {
        log_debug(LD_EXIT, "eventdns said that %s resolves to %s",
                  safe_str(escaped_address),
                  escaped_safe_str(answer_buf));
      }
      tor_free(escaped_address);
    } else if (type == DNS_IPv6_AAAA && count) {
      char answer_buf[TOR_ADDR_BUF_LEN];
      char *escaped_address;
      struct in6_addr *addrs = addresses;
      tor_addr_from_in6(&addr, &addrs[0]);
      tor_inet_ntop(AF_INET6, &addrs[0], answer_buf, sizeof(answer_buf));
      escaped_address = esc_for_log(string_address);

      if (answer_is_wildcarded(answer_buf)) {
        log_debug(LD_EXIT, "eventdns said that %s resolves to ISP-hijacked "
                  "address %s; treating as a failure.",
                  safe_str(escaped_address),
                  escaped_safe_str(answer_buf));
        was_wildcarded = 1;
        tor_addr_make_unspec(&addr);
        result = DNS_ERR_NOTEXIST;
      } else {
        log_debug(LD_EXIT, "eventdns said that %s resolves to %s",
                  safe_str(escaped_address),
                  escaped_safe_str(answer_buf));
      }
      tor_free(escaped_address);
    } else if (type == DNS_PTR && count) {
      char *escaped_address;
      hostname = ((char**)addresses)[0];
      escaped_address = esc_for_log(string_address);
      log_debug(LD_EXIT, "eventdns said that %s resolves to %s",
                safe_str(escaped_address),
                escaped_safe_str(hostname));
      tor_free(escaped_address);
    } else if (count) {
      log_warn(LD_EXIT, "eventdns returned only non-IPv4 answers for %s.",
               escaped_safe_str(string_address));
    } else {
      log_warn(LD_BUG, "eventdns returned no addresses or error for %s!",
               escaped_safe_str(string_address));
    }
  }
  if (was_wildcarded) {
    if (is_test_address(string_address)) {
      /* Ick.  We're getting redirected on known-good addresses.  Our DNS
       * server must really hate us.  */
      add_wildcarded_test_address(string_address);
    }
  }

  if (orig_query_type && type && orig_query_type != type) {
    log_warn(LD_BUG, "Weird; orig_query_type == %d but type == %d",
             (int)orig_query_type, (int)type);
  }
  if (result != DNS_ERR_SHUTDOWN)
    dns_found_answer(string_address, orig_query_type,
                     result, &addr, hostname, ttl);

  tor_free(arg_);
}

/** Start a single DNS resolve for <b>address</b> (if <b>query_type</b> is
 * DNS_IPv4_A or DNS_IPv6_AAAA) <b>ptr_address</b> (if <b>query_type</b> is
 * DNS_PTR). Return 0 if we launched the request, -1 otherwise. */
static int
launch_one_resolve(const char *address, uint8_t query_type,
                   const tor_addr_t *ptr_address)
{
  const int options = get_options()->ServerDNSSearchDomains ? 0
    : DNS_QUERY_NO_SEARCH;
  const size_t addr_len = strlen(address);
  struct evdns_request *req = 0;
  char *addr = tor_malloc(addr_len + 2);
  addr[0] = (char) query_type;
  memcpy(addr+1, address, addr_len + 1);

  switch (query_type) {
  case DNS_IPv4_A:
    req = evdns_base_resolve_ipv4(the_evdns_base,
                                  address, options, evdns_callback, addr);
    break;
  case DNS_IPv6_AAAA:
    req = evdns_base_resolve_ipv6(the_evdns_base,
                                  address, options, evdns_callback, addr);
    ++n_ipv6_requests_made;
    break;
  case DNS_PTR:
    if (tor_addr_family(ptr_address) == AF_INET)
      req = evdns_base_resolve_reverse(the_evdns_base,
                                       tor_addr_to_in(ptr_address),
                                       DNS_QUERY_NO_SEARCH,
                                       evdns_callback, addr);
    else if (tor_addr_family(ptr_address) == AF_INET6)
      req = evdns_base_resolve_reverse_ipv6(the_evdns_base,
                                            tor_addr_to_in6(ptr_address),
                                            DNS_QUERY_NO_SEARCH,
                                            evdns_callback, addr);
    else
      log_warn(LD_BUG, "Called with PTR query and unexpected address family");
    break;
  default:
    log_warn(LD_BUG, "Called with unexpectd query type %d", (int)query_type);
    break;
  }

  if (req) {
    return 0;
  } else {
    tor_free(addr);
    return -1;
  }
}

/** For eventdns: start resolving as necessary to find the target for
 * <b>exitconn</b>.  Returns -1 on error, -2 on transient error,
 * 0 on "resolve launched." */
static int
launch_resolve(cached_resolve_t *resolve)
{
  tor_addr_t a;
  int r;

  if (get_options()->DisableNetwork)
    return -1;

  /* What? Nameservers not configured?  Sounds like a bug. */
  if (!nameservers_configured) {
    log_warn(LD_EXIT, "(Harmless.) Nameservers not configured, but resolve "
             "launched.  Configuring.");
    if (configure_nameservers(1) < 0) {
      return -1;
    }
  }

  r = tor_addr_parse_PTR_name(
                            &a, resolve->address, AF_UNSPEC, 0);

  tor_assert(the_evdns_base);
  if (r == 0) {
    log_info(LD_EXIT, "Launching eventdns request for %s",
             escaped_safe_str(resolve->address));
    resolve->res_status_ipv4 = RES_STATUS_INFLIGHT;
    if (get_options()->IPv6Exit)
      resolve->res_status_ipv6 = RES_STATUS_INFLIGHT;

    if (launch_one_resolve(resolve->address, DNS_IPv4_A, NULL) < 0) {
      resolve->res_status_ipv4 = 0;
      r = -1;
    }

    if (r==0 && get_options()->IPv6Exit) {
      /* We ask for an IPv6 address for *everything*. */
      if (launch_one_resolve(resolve->address, DNS_IPv6_AAAA, NULL) < 0) {
        resolve->res_status_ipv6 = 0;
        r = -1;
      }
    }
  } else if (r == 1) {
    r = 0;
    log_info(LD_EXIT, "Launching eventdns reverse request for %s",
             escaped_safe_str(resolve->address));
    resolve->res_status_hostname = RES_STATUS_INFLIGHT;
    if (launch_one_resolve(resolve->address, DNS_PTR, &a) < 0) {
      resolve->res_status_hostname = 0;
      r = -1;
    }
  } else if (r == -1) {
    log_warn(LD_BUG, "Somehow a malformed in-addr.arpa address reached here.");
  }

  if (r < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_EXIT, "eventdns rejected address %s.",
           escaped_safe_str(resolve->address));
  }
  return r;
}

/** How many requests for bogus addresses have we launched so far? */
static int n_wildcard_requests = 0;

/** Map from dotted-quad IP address in response to an int holding how many
 * times we've seen it for a randomly generated (hopefully bogus) address.  It
 * would be easier to use definitely-invalid addresses (as specified by
 * RFC2606), but see comment in dns_launch_wildcard_checks(). */
static strmap_t *dns_wildcard_response_count = NULL;

/** If present, a list of dotted-quad IP addresses that we are pretty sure our
 * nameserver wants to return in response to requests for nonexistent domains.
 */
static smartlist_t *dns_wildcard_list = NULL;
/** True iff we've logged about a single address getting wildcarded.
 * Subsequent warnings will be less severe.  */
static int dns_wildcard_one_notice_given = 0;
/** True iff we've warned that our DNS server is wildcarding too many failures.
 */
static int dns_wildcard_notice_given = 0;

/** List of supposedly good addresses that are getting wildcarded to the
 * same addresses as nonexistent addresses. */
static smartlist_t *dns_wildcarded_test_address_list = NULL;
/** True iff we've warned about a test address getting wildcarded */
static int dns_wildcarded_test_address_notice_given = 0;
/** True iff all addresses seem to be getting wildcarded. */
static int dns_is_completely_invalid = 0;

/** Called when we see <b>id</b> (a dotted quad or IPv6 address) in response
 * to a request for a hopefully bogus address. */
static void
wildcard_increment_answer(const char *id)
{
  int *ip;
  if (!dns_wildcard_response_count)
    dns_wildcard_response_count = strmap_new();

  ip = strmap_get(dns_wildcard_response_count, id); // may be null (0)
  if (!ip) {
    ip = tor_malloc_zero(sizeof(int));
    strmap_set(dns_wildcard_response_count, id, ip);
  }
  ++*ip;

  if (*ip > 5 && n_wildcard_requests > 10) {
    if (!dns_wildcard_list) dns_wildcard_list = smartlist_new();
    if (!smartlist_contains_string(dns_wildcard_list, id)) {
    tor_log(dns_wildcard_notice_given ? LOG_INFO : LOG_NOTICE, LD_EXIT,
        "Your DNS provider has given \"%s\" as an answer for %d different "
        "invalid addresses. Apparently they are hijacking DNS failures. "
        "I'll try to correct for this by treating future occurrences of "
        "\"%s\" as 'not found'.", id, *ip, id);
      smartlist_add(dns_wildcard_list, tor_strdup(id));
    }
    if (!dns_wildcard_notice_given)
      control_event_server_status(LOG_NOTICE, "DNS_HIJACKED");
    dns_wildcard_notice_given = 1;
  }
}

/** Note that a single test address (one believed to be good) seems to be
 * getting redirected to the same IP as failures are. */
static void
add_wildcarded_test_address(const char *address)
{
  int n, n_test_addrs;
  if (!dns_wildcarded_test_address_list)
    dns_wildcarded_test_address_list = smartlist_new();

  if (smartlist_contains_string_case(dns_wildcarded_test_address_list,
                                     address))
    return;

  n_test_addrs = get_options()->ServerDNSTestAddresses ?
    smartlist_len(get_options()->ServerDNSTestAddresses) : 0;

  smartlist_add(dns_wildcarded_test_address_list, tor_strdup(address));
  n = smartlist_len(dns_wildcarded_test_address_list);
  if (n > n_test_addrs/2) {
    tor_log(dns_wildcarded_test_address_notice_given ? LOG_INFO : LOG_NOTICE,
        LD_EXIT, "Your DNS provider tried to redirect \"%s\" to a junk "
        "address.  It has done this with %d test addresses so far.  I'm "
        "going to stop being an exit node for now, since our DNS seems so "
        "broken.", address, n);
    if (!dns_is_completely_invalid) {
      dns_is_completely_invalid = 1;
      mark_my_descriptor_dirty("dns hijacking confirmed");
    }
    if (!dns_wildcarded_test_address_notice_given)
      control_event_server_status(LOG_WARN, "DNS_USELESS");
    dns_wildcarded_test_address_notice_given = 1;
  }
}

/** Callback function when we get an answer (possibly failing) for a request
 * for a (hopefully) nonexistent domain. */
static void
evdns_wildcard_check_callback(int result, char type, int count, int ttl,
                              void *addresses, void *arg)
{
  (void)ttl;
  ++n_wildcard_requests;
  if (result == DNS_ERR_NONE && count) {
    char *string_address = arg;
    int i;
    if (type == DNS_IPv4_A) {
      const uint32_t *addrs = addresses;
      for (i = 0; i < count; ++i) {
        char answer_buf[INET_NTOA_BUF_LEN+1];
        struct in_addr in;
        in.s_addr = addrs[i];
        tor_inet_ntoa(&in, answer_buf, sizeof(answer_buf));
        wildcard_increment_answer(answer_buf);
      }
    } else if (type == DNS_IPv6_AAAA) {
      const struct in6_addr *addrs = addresses;
      for (i = 0; i < count; ++i) {
        char answer_buf[TOR_ADDR_BUF_LEN+1];
        tor_inet_ntop(AF_INET6, &addrs[i], answer_buf, sizeof(answer_buf));
        wildcard_increment_answer(answer_buf);
      }
    }

    tor_log(dns_wildcard_one_notice_given ? LOG_INFO : LOG_NOTICE, LD_EXIT,
        "Your DNS provider gave an answer for \"%s\", which "
        "is not supposed to exist. Apparently they are hijacking "
        "DNS failures. Trying to correct for this. We've noticed %d "
        "possibly bad address%s so far.",
        string_address, strmap_size(dns_wildcard_response_count),
        (strmap_size(dns_wildcard_response_count) == 1) ? "" : "es");
    dns_wildcard_one_notice_given = 1;
  }
  tor_free(arg);
}

/** Launch a single request for a nonexistent hostname consisting of between
 * <b>min_len</b> and <b>max_len</b> random (plausible) characters followed by
 * <b>suffix</b> */
static void
launch_wildcard_check(int min_len, int max_len, int is_ipv6,
                      const char *suffix)
{
  char *addr;
  struct evdns_request *req;

  addr = crypto_random_hostname(min_len, max_len, "", suffix);
  log_info(LD_EXIT, "Testing whether our DNS server is hijacking nonexistent "
           "domains with request for bogus hostname \"%s\"", addr);

  tor_assert(the_evdns_base);
  if (is_ipv6)
    req = evdns_base_resolve_ipv6(
                         the_evdns_base,
                         /* This "addr" tells us which address to resolve */
                         addr,
                         DNS_QUERY_NO_SEARCH, evdns_wildcard_check_callback,
                         /* This "addr" is an argument to the callback*/ addr);
  else
    req = evdns_base_resolve_ipv4(
                         the_evdns_base,
                         /* This "addr" tells us which address to resolve */
                         addr,
                         DNS_QUERY_NO_SEARCH, evdns_wildcard_check_callback,
                         /* This "addr" is an argument to the callback*/ addr);
  if (!req) {
    /* There is no evdns request in progress; stop addr from getting leaked */
    tor_free(addr);
  }
}

/** Launch attempts to resolve a bunch of known-good addresses (configured in
 * ServerDNSTestAddresses).  [Callback for a libevent timer] */
static void
launch_test_addresses(evutil_socket_t fd, short event, void *args)
{
  const or_options_t *options = get_options();
  (void)fd;
  (void)event;
  (void)args;

  if (options->DisableNetwork)
    return;

  log_info(LD_EXIT, "Launching checks to see whether our nameservers like to "
           "hijack *everything*.");
  /* This situation is worse than the failure-hijacking situation.  When this
   * happens, we're no good for DNS requests at all, and we shouldn't really
   * be an exit server.*/
  if (options->ServerDNSTestAddresses) {

    tor_assert(the_evdns_base);
    SMARTLIST_FOREACH_BEGIN(options->ServerDNSTestAddresses,
                            const char *, address) {
      if (launch_one_resolve(address, DNS_IPv4_A, NULL) < 0) {
        log_info(LD_EXIT, "eventdns rejected test address %s",
                 escaped_safe_str(address));
      }

      if (launch_one_resolve(address, DNS_IPv6_AAAA, NULL) < 0) {
        log_info(LD_EXIT, "eventdns rejected test address %s",
                 escaped_safe_str(address));
      }
    } SMARTLIST_FOREACH_END(address);
  }
}

#define N_WILDCARD_CHECKS 2

/** Launch DNS requests for a few nonexistent hostnames and a few well-known
 * hostnames, and see if we can catch our nameserver trying to hijack them and
 * map them to a stupid "I couldn't find ggoogle.com but maybe you'd like to
 * buy these lovely encyclopedias" page. */
static void
dns_launch_wildcard_checks(void)
{
  int i, ipv6;
  log_info(LD_EXIT, "Launching checks to see whether our nameservers like "
           "to hijack DNS failures.");
  for (ipv6 = 0; ipv6 <= 1; ++ipv6) {
    for (i = 0; i < N_WILDCARD_CHECKS; ++i) {
      /* RFC2606 reserves these.  Sadly, some DNS hijackers, in a silly
       * attempt to 'comply' with rfc2606, refrain from giving A records for
       * these.  This is the standards-compliance equivalent of making sure
       * that your crackhouse's elevator inspection certificate is up to date.
       */
      launch_wildcard_check(2, 16, ipv6, ".invalid");
      launch_wildcard_check(2, 16, ipv6, ".test");

      /* These will break specs if there are ever any number of
       * 8+-character top-level domains. */
      launch_wildcard_check(8, 16, ipv6, "");

      /* Try some random .com/org/net domains. This will work fine so long as
       * not too many resolve to the same place. */
      launch_wildcard_check(8, 16, ipv6, ".com");
      launch_wildcard_check(8, 16, ipv6, ".org");
      launch_wildcard_check(8, 16, ipv6, ".net");
  }
  }
}

/** If appropriate, start testing whether our DNS servers tend to lie to
 * us. */
void
dns_launch_correctness_checks(void)
{
  static struct event *launch_event = NULL;
  struct timeval timeout;
  if (!get_options()->ServerDNSDetectHijacking)
    return;
  dns_launch_wildcard_checks();

  /* Wait a while before launching requests for test addresses, so we can
   * get the results from checking for wildcarding. */
  if (! launch_event)
    launch_event = tor_evtimer_new(tor_libevent_get_base(),
                                   launch_test_addresses, NULL);
  timeout.tv_sec = 30;
  timeout.tv_usec = 0;
  if (evtimer_add(launch_event, &timeout)<0) {
    log_warn(LD_BUG, "Couldn't add timer for checking for dns hijacking");
  }
}

/** Return true iff our DNS servers lie to us too much to be trusted. */
int
dns_seems_to_be_broken(void)
{
  return dns_is_completely_invalid;
}

/** Return true iff we think that IPv6 hostname lookup is broken */
int
dns_seems_to_be_broken_for_ipv6(void)
{
  return dns_is_broken_for_ipv6;
}

/** Forget what we've previously learned about our DNS servers' correctness. */
void
dns_reset_correctness_checks(void)
{
  strmap_free(dns_wildcard_response_count, tor_free_);
  dns_wildcard_response_count = NULL;

  n_wildcard_requests = 0;

  n_ipv6_requests_made = n_ipv6_timeouts = 0;

  if (dns_wildcard_list) {
    SMARTLIST_FOREACH(dns_wildcard_list, char *, cp, tor_free(cp));
    smartlist_clear(dns_wildcard_list);
  }
  if (dns_wildcarded_test_address_list) {
    SMARTLIST_FOREACH(dns_wildcarded_test_address_list, char *, cp,
                      tor_free(cp));
    smartlist_clear(dns_wildcarded_test_address_list);
  }
  dns_wildcard_one_notice_given = dns_wildcard_notice_given =
    dns_wildcarded_test_address_notice_given = dns_is_completely_invalid =
    dns_is_broken_for_ipv6 = 0;
}

/** Return true iff we have noticed that the dotted-quad <b>ip</b> has been
 * returned in response to requests for nonexistent hostnames. */
static int
answer_is_wildcarded(const char *ip)
{
  return dns_wildcard_list && smartlist_contains_string(dns_wildcard_list, ip);
}

/** Exit with an assertion if <b>resolve</b> is corrupt. */
static void
assert_resolve_ok(cached_resolve_t *resolve)
{
  tor_assert(resolve);
  tor_assert(resolve->magic == CACHED_RESOLVE_MAGIC);
  tor_assert(strlen(resolve->address) < MAX_ADDRESSLEN);
  tor_assert(tor_strisnonupper(resolve->address));
  if (resolve->state != CACHE_STATE_PENDING) {
    tor_assert(!resolve->pending_connections);
  }
  if (resolve->state == CACHE_STATE_PENDING ||
      resolve->state == CACHE_STATE_DONE) {
#if 0
    tor_assert(!resolve->ttl);
    if (resolve->is_reverse)
      tor_assert(!resolve->hostname);
    else
      tor_assert(!resolve->result_ipv4.addr_ipv4);
#endif
    /*XXXXX ADD MORE */
  }
}

/** Return the number of DNS cache entries as an int */
static int
dns_cache_entry_count(void)
{
   return HT_SIZE(&cache_root);
}

/** Log memory information about our internal DNS cache at level 'severity'. */
void
dump_dns_mem_usage(int severity)
{
  /* This should never be larger than INT_MAX. */
  int hash_count = dns_cache_entry_count();
  size_t hash_mem = sizeof(struct cached_resolve_t) * hash_count;
  hash_mem += HT_MEM_USAGE(&cache_root);

  /* Print out the count and estimated size of our &cache_root.  It undercounts
     hostnames in cached reverse resolves.
   */
  tor_log(severity, LD_MM, "Our DNS cache has %d entries.", hash_count);
  tor_log(severity, LD_MM, "Our DNS cache size is approximately %u bytes.",
      (unsigned)hash_mem);
}

#ifdef DEBUG_DNS_CACHE
/** Exit with an assertion if the DNS cache is corrupt. */
static void
assert_cache_ok_(void)
{
  cached_resolve_t **resolve;
  int bad_rep = HT_REP_IS_BAD_(cache_map, &cache_root);
  if (bad_rep) {
    log_err(LD_BUG, "Bad rep type %d on dns cache hash table", bad_rep);
    tor_assert(!bad_rep);
  }

  HT_FOREACH(resolve, cache_map, &cache_root) {
    assert_resolve_ok(*resolve);
    tor_assert((*resolve)->state != CACHE_STATE_DONE);
  }
  if (!cached_resolve_pqueue)
    return;

  smartlist_pqueue_assert_ok(cached_resolve_pqueue,
                             compare_cached_resolves_by_expiry_,
                             STRUCT_OFFSET(cached_resolve_t, minheap_idx));

  SMARTLIST_FOREACH(cached_resolve_pqueue, cached_resolve_t *, res,
    {
      if (res->state == CACHE_STATE_DONE) {
        cached_resolve_t *found = HT_FIND(cache_map, &cache_root, res);
        tor_assert(!found || found != res);
      } else {
        cached_resolve_t *found = HT_FIND(cache_map, &cache_root, res);
        tor_assert(found);
      }
    });
}
#endif

