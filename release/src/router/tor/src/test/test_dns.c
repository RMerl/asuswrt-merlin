#include "or.h"
#include "test.h"

#define DNS_PRIVATE

#include "dns.h"
#include "connection.h"
#include "router.h"

#define NS_MODULE dns

#define NS_SUBMODULE clip_ttl

static void
NS(test_main)(void *arg)
{
  (void)arg;

  uint32_t ttl_mid = MIN_DNS_TTL / 2 + MAX_DNS_TTL / 2;

  tt_int_op(dns_clip_ttl(MIN_DNS_TTL - 1),==,MIN_DNS_TTL);
  tt_int_op(dns_clip_ttl(ttl_mid),==,ttl_mid);
  tt_int_op(dns_clip_ttl(MAX_DNS_TTL + 1),==,MAX_DNS_TTL);

  done:
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE expiry_ttl

static void
NS(test_main)(void *arg)
{
  (void)arg;

  uint32_t ttl_mid = MIN_DNS_TTL / 2 + MAX_DNS_ENTRY_AGE / 2;

  tt_int_op(dns_get_expiry_ttl(MIN_DNS_TTL - 1),==,MIN_DNS_TTL);
  tt_int_op(dns_get_expiry_ttl(ttl_mid),==,ttl_mid);
  tt_int_op(dns_get_expiry_ttl(MAX_DNS_ENTRY_AGE + 1),==,MAX_DNS_ENTRY_AGE);

  done:
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE resolve

static int resolve_retval = 0;
static int resolve_made_conn_pending = 0;
static char *resolved_name = NULL;
static cached_resolve_t *cache_entry_mock = NULL;

static int n_fake_impl = 0;

NS_DECL(int, dns_resolve_impl, (edge_connection_t *exitconn, int is_resolve,
                                or_circuit_t *oncirc, char **hostname_out,
                                int *made_connection_pending_out,
                                cached_resolve_t **resolve_out));

/** This will be our configurable substitute for <b>dns_resolve_impl</b> in
 * dns.c. It will return <b>resolve_retval</b>,
 * and set <b>resolve_made_conn_pending</b> to
 * <b>made_connection_pending_out</b>. It will set <b>hostname_out</b>
 * to a duplicate of <b>resolved_name</b> and it will set <b>resolve_out</b>
 * to <b>cache_entry</b>. Lastly, it will increment <b>n_fake_impl</b< by
 * 1.
 */
static int
NS(dns_resolve_impl)(edge_connection_t *exitconn, int is_resolve,
                     or_circuit_t *oncirc, char **hostname_out,
                     int *made_connection_pending_out,
                     cached_resolve_t **resolve_out)
{
  (void)oncirc;
  (void)exitconn;
  (void)is_resolve;

  if (made_connection_pending_out)
    *made_connection_pending_out = resolve_made_conn_pending;

  if (hostname_out && resolved_name)
    *hostname_out = tor_strdup(resolved_name);

  if (resolve_out && cache_entry_mock)
    *resolve_out = cache_entry_mock;

  n_fake_impl++;

  return resolve_retval;
}

static edge_connection_t *conn_for_resolved_cell = NULL;

static int n_send_resolved_cell_replacement = 0;
static uint8_t last_answer_type = 0;
static cached_resolve_t *last_resolved;

static void
NS(send_resolved_cell)(edge_connection_t *conn, uint8_t answer_type,
                       const cached_resolve_t *resolved)
{
  conn_for_resolved_cell = conn;

  last_answer_type = answer_type;
  last_resolved = (cached_resolve_t *)resolved;

  n_send_resolved_cell_replacement++;
}

static int n_send_resolved_hostname_cell_replacement = 0;

static char *last_resolved_hostname = NULL;

static void
NS(send_resolved_hostname_cell)(edge_connection_t *conn,
                                const char *hostname)
{
  conn_for_resolved_cell = conn;

  tor_free(last_resolved_hostname);
  last_resolved_hostname = tor_strdup(hostname);

  n_send_resolved_hostname_cell_replacement++;
}

static int n_dns_cancel_pending_resolve_replacement = 0;

static void
NS(dns_cancel_pending_resolve)(const char *address)
{
  (void) address;
  n_dns_cancel_pending_resolve_replacement++;
}

static int n_connection_free = 0;
static connection_t *last_freed_conn = NULL;

static void
NS(connection_free)(connection_t *conn)
{
   n_connection_free++;

   last_freed_conn = conn;
}

static void
NS(test_main)(void *arg)
{
  (void) arg;
  int retval;
  int prev_n_send_resolved_hostname_cell_replacement;
  int prev_n_send_resolved_cell_replacement;
  int prev_n_connection_free;
  cached_resolve_t *fake_resolved = tor_malloc(sizeof(cached_resolve_t));
  edge_connection_t *exitconn = tor_malloc(sizeof(edge_connection_t));
  edge_connection_t *nextconn = tor_malloc(sizeof(edge_connection_t));

  or_circuit_t *on_circuit = tor_malloc(sizeof(or_circuit_t));
  memset(on_circuit,0,sizeof(or_circuit_t));
  on_circuit->base_.magic = OR_CIRCUIT_MAGIC;

  memset(fake_resolved,0,sizeof(cached_resolve_t));
  memset(exitconn,0,sizeof(edge_connection_t));
  memset(nextconn,0,sizeof(edge_connection_t));

  NS_MOCK(dns_resolve_impl);
  NS_MOCK(send_resolved_cell);
  NS_MOCK(send_resolved_hostname_cell);

  /*
   * CASE 1: dns_resolve_impl returns 1 and sets a hostname. purpose is
   * EXIT_PURPOSE_RESOLVE.
   *
   * We want dns_resolve() to call send_resolved_hostname_cell() for a
   * given exit connection (represented by edge_connection_t object)
   * with a hostname it received from _impl.
   */

  prev_n_send_resolved_hostname_cell_replacement =
  n_send_resolved_hostname_cell_replacement;

  exitconn->base_.purpose = EXIT_PURPOSE_RESOLVE;
  exitconn->on_circuit = &(on_circuit->base_);

  resolve_retval = 1;
  resolved_name = tor_strdup("www.torproject.org");

  retval = dns_resolve(exitconn);

  tt_int_op(retval,==,1);
  tt_str_op(resolved_name,==,last_resolved_hostname);
  tt_assert(conn_for_resolved_cell == exitconn);
  tt_int_op(n_send_resolved_hostname_cell_replacement,==,
            prev_n_send_resolved_hostname_cell_replacement + 1);
  tt_assert(exitconn->on_circuit == NULL);

  tor_free(last_resolved_hostname);
  // implies last_resolved_hostname = NULL;

  /* CASE 2: dns_resolve_impl returns 1, but does not set hostname.
   * Instead, it yields cached_resolve_t object.
   *
   * We want dns_resolve to call send_resolved_cell on exitconn with
   * RESOLVED_TYPE_AUTO and the cached_resolve_t object from _impl.
   */

  tor_free(resolved_name);
  resolved_name = NULL;

  exitconn->on_circuit = &(on_circuit->base_);

  cache_entry_mock = fake_resolved;

  prev_n_send_resolved_cell_replacement =
  n_send_resolved_cell_replacement;

  retval = dns_resolve(exitconn);

  tt_int_op(retval,==,1);
  tt_assert(conn_for_resolved_cell == exitconn);
  tt_int_op(n_send_resolved_cell_replacement,==,
            prev_n_send_resolved_cell_replacement + 1);
  tt_assert(last_resolved == fake_resolved);
  tt_int_op(last_answer_type,==,0xff);
  tt_assert(exitconn->on_circuit == NULL);

  /* CASE 3: The purpose of exit connection is not EXIT_PURPOSE_RESOLVE
   * and _impl returns 1.
   *
   * We want dns_resolve to prepend exitconn to n_streams linked list.
   * We don't want it to send any cells about hostname being resolved.
   */

  exitconn->base_.purpose = EXIT_PURPOSE_CONNECT;
  exitconn->on_circuit = &(on_circuit->base_);

  on_circuit->n_streams = nextconn;

  prev_n_send_resolved_cell_replacement =
  n_send_resolved_cell_replacement;

  prev_n_send_resolved_hostname_cell_replacement =
  n_send_resolved_hostname_cell_replacement;

  retval = dns_resolve(exitconn);

  tt_int_op(retval,==,1);
  tt_assert(on_circuit->n_streams == exitconn);
  tt_assert(exitconn->next_stream == nextconn);
  tt_int_op(prev_n_send_resolved_cell_replacement,==,
            n_send_resolved_cell_replacement);
  tt_int_op(prev_n_send_resolved_hostname_cell_replacement,==,
            n_send_resolved_hostname_cell_replacement);

  /* CASE 4: _impl returns 0.
   *
   * We want dns_resolve() to set exitconn state to
   * EXIT_CONN_STATE_RESOLVING and prepend exitconn to resolving_streams
   * linked list.
   */

  exitconn->on_circuit = &(on_circuit->base_);

  resolve_retval = 0;

  exitconn->next_stream = NULL;
  on_circuit->resolving_streams = nextconn;

  retval = dns_resolve(exitconn);

  tt_int_op(retval,==,0);
  tt_int_op(exitconn->base_.state,==,EXIT_CONN_STATE_RESOLVING);
  tt_assert(on_circuit->resolving_streams == exitconn);
  tt_assert(exitconn->next_stream == nextconn);

  /* CASE 5: _impl returns -1 when purpose of exitconn is
   * EXIT_PURPOSE_RESOLVE. We want dns_resolve to call send_resolved_cell
   * on exitconn with type being RESOLVED_TYPE_ERROR.
   */

  NS_MOCK(dns_cancel_pending_resolve);
  NS_MOCK(connection_free);

  exitconn->on_circuit = &(on_circuit->base_);
  exitconn->base_.purpose = EXIT_PURPOSE_RESOLVE;

  resolve_retval = -1;

  prev_n_send_resolved_cell_replacement =
  n_send_resolved_cell_replacement;

  prev_n_connection_free = n_connection_free;

  retval = dns_resolve(exitconn);

  tt_int_op(retval,==,-1);
  tt_int_op(n_send_resolved_cell_replacement,==,
            prev_n_send_resolved_cell_replacement + 1);
  tt_int_op(last_answer_type,==,RESOLVED_TYPE_ERROR);
  tt_int_op(n_dns_cancel_pending_resolve_replacement,==,1);
  tt_int_op(n_connection_free,==,prev_n_connection_free + 1);
  tt_assert(last_freed_conn == TO_CONN(exitconn));

  done:
  NS_UNMOCK(dns_resolve_impl);
  NS_UNMOCK(send_resolved_cell);
  NS_UNMOCK(send_resolved_hostname_cell);
  NS_UNMOCK(dns_cancel_pending_resolve);
  NS_UNMOCK(connection_free);
  tor_free(on_circuit);
  tor_free(exitconn);
  tor_free(nextconn);
  tor_free(resolved_name);
  tor_free(fake_resolved);
  tor_free(last_resolved_hostname);
  return;
}

#undef NS_SUBMODULE

/** Create an <b>edge_connection_t</b> instance that is considered a
 * valid exit connection by asserts in dns_resolve_impl.
 */
static edge_connection_t *
create_valid_exitconn(void)
{
  edge_connection_t *exitconn = tor_malloc_zero(sizeof(edge_connection_t));
  TO_CONN(exitconn)->type = CONN_TYPE_EXIT;
  TO_CONN(exitconn)->magic = EDGE_CONNECTION_MAGIC;
  TO_CONN(exitconn)->purpose = EXIT_PURPOSE_RESOLVE;
  TO_CONN(exitconn)->state = EXIT_CONN_STATE_RESOLVING;
  exitconn->base_.s = TOR_INVALID_SOCKET;

  return exitconn;
}

#define NS_SUBMODULE ASPECT(resolve_impl, addr_is_ip_no_need_to_resolve)

/*
 * Given that <b>exitconn->base_.address</b> is IP address string, we
 * want dns_resolve_impl() to parse it and store in
 * <b>exitconn->base_.addr</b>. We expect dns_resolve_impl to return 1.
 * Lastly, we want it to set the TTL value to default one for DNS queries.
 */

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending;
  const tor_addr_t *resolved_addr;
  tor_addr_t addr_to_compare;

  (void)arg;

  tor_addr_parse(&addr_to_compare, "8.8.8.8");

  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  edge_connection_t *exitconn = create_valid_exitconn();

  TO_CONN(exitconn)->address = tor_strdup("8.8.8.8");

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  resolved_addr = &(exitconn->base_.addr);

  tt_int_op(retval,==,1);
  tt_assert(tor_addr_eq(resolved_addr, (const tor_addr_t *)&addr_to_compare));
  tt_int_op(exitconn->address_ttl,==,DEFAULT_DNS_TTL);

  done:
  tor_free(on_circ);
  tor_free(TO_CONN(exitconn)->address);
  tor_free(exitconn);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, non_exit)

/** Given that Tor instance is not configured as an exit node, we want
 * dns_resolve_impl() to fail with return value -1.
 */
static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 1;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  (void)arg;

  TO_CONN(exitconn)->address = tor_strdup("torproject.org");

  NS_MOCK(router_my_exit_policy_is_reject_star);

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,-1);

  done:
  tor_free(TO_CONN(exitconn)->address);
  tor_free(exitconn);
  tor_free(on_circ);
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, addr_is_invalid_dest)

/** Given that address is not a valid destination (as judged by
 * address_is_invalid_destination() function), we want dns_resolve_impl()
 * function to fail with return value -1.
 */

static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 0;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  (void)arg;

  NS_MOCK(router_my_exit_policy_is_reject_star);

  TO_CONN(exitconn)->address = tor_strdup("invalid#@!.org");

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,-1);

  done:
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  tor_free(TO_CONN(exitconn)->address);
  tor_free(exitconn);
  tor_free(on_circ);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, malformed_ptr)

/** Given that address is a malformed PTR name, we want dns_resolve_impl to
 * fail.
 */

static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 0;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  (void)arg;

  TO_CONN(exitconn)->address = tor_strdup("1.0.0.127.in-addr.arpa");

  NS_MOCK(router_my_exit_policy_is_reject_star);

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,-1);

  tor_free(TO_CONN(exitconn)->address);

  TO_CONN(exitconn)->address =
  tor_strdup("z01234567890123456789.in-addr.arpa");

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,-1);

  done:
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  tor_free(TO_CONN(exitconn)->address);
  tor_free(exitconn);
  tor_free(on_circ);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, cache_hit_pending)

/* Given that there is already a pending resolve for the given address,
 * we want dns_resolve_impl to append our exit connection to list
 * of pending connections for the pending DNS request and return 0.
 */

static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 0;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending = 0;

  pending_connection_t *pending_conn = NULL;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  cached_resolve_t *cache_entry = tor_malloc_zero(sizeof(cached_resolve_t));
  cache_entry->magic = CACHED_RESOLVE_MAGIC;
  cache_entry->state = CACHE_STATE_PENDING;
  cache_entry->minheap_idx = -1;
  cache_entry->expire = time(NULL) + 60 * 60;

  (void)arg;

  TO_CONN(exitconn)->address = tor_strdup("torproject.org");

  strlcpy(cache_entry->address, TO_CONN(exitconn)->address,
          sizeof(cache_entry->address));

  NS_MOCK(router_my_exit_policy_is_reject_star);

  dns_init();

  dns_insert_cache_entry(cache_entry);

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,0);
  tt_int_op(made_pending,==,1);

  pending_conn = cache_entry->pending_connections;

  tt_assert(pending_conn != NULL);
  tt_assert(pending_conn->conn == exitconn);

  done:
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  tor_free(on_circ);
  tor_free(TO_CONN(exitconn)->address);
  tor_free(cache_entry->pending_connections);
  tor_free(cache_entry);
  tor_free(exitconn);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, cache_hit_cached)

/* Given that a finished DNS resolve is available in our cache, we want
 * dns_resolve_impl() return it to called via resolve_out and pass the
 * handling to set_exitconn_info_from_resolve function.
 */
static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 0;
}

static edge_connection_t *last_exitconn = NULL;
static cached_resolve_t *last_resolve = NULL;

static int
NS(set_exitconn_info_from_resolve)(edge_connection_t *exitconn,
                                   const cached_resolve_t *resolve,
                                   char **hostname_out)
{
  last_exitconn = exitconn;
  last_resolve = (cached_resolve_t *)resolve;

  (void)hostname_out;

  return 0;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending = 0;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  cached_resolve_t *resolve_out = NULL;

  cached_resolve_t *cache_entry = tor_malloc_zero(sizeof(cached_resolve_t));
  cache_entry->magic = CACHED_RESOLVE_MAGIC;
  cache_entry->state = CACHE_STATE_CACHED;
  cache_entry->minheap_idx = -1;
  cache_entry->expire = time(NULL) + 60 * 60;

  (void)arg;

  TO_CONN(exitconn)->address = tor_strdup("torproject.org");

  strlcpy(cache_entry->address, TO_CONN(exitconn)->address,
          sizeof(cache_entry->address));

  NS_MOCK(router_my_exit_policy_is_reject_star);
  NS_MOCK(set_exitconn_info_from_resolve);

  dns_init();

  dns_insert_cache_entry(cache_entry);

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            &resolve_out);

  tt_int_op(retval,==,0);
  tt_int_op(made_pending,==,0);
  tt_assert(resolve_out == cache_entry);

  tt_assert(last_exitconn == exitconn);
  tt_assert(last_resolve == cache_entry);

  done:
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  NS_UNMOCK(set_exitconn_info_from_resolve);
  tor_free(on_circ);
  tor_free(TO_CONN(exitconn)->address);
  tor_free(cache_entry->pending_connections);
  tor_free(cache_entry);
  return;
}

#undef NS_SUBMODULE

#define NS_SUBMODULE ASPECT(resolve_impl, cache_miss)

/* Given that there are neither pending nor pre-cached resolve for a given
 * address, we want dns_resolve_impl() to create a new cached_resolve_t
 * object, mark it as pending, insert it into the cache, attach the exit
 * connection to list of pending connections and call launch_resolve()
 * with the cached_resolve_t object it created.
 */
static int
NS(router_my_exit_policy_is_reject_star)(void)
{
  return 0;
}

static cached_resolve_t *last_launched_resolve = NULL;

static int
NS(launch_resolve)(cached_resolve_t *resolve)
{
  last_launched_resolve = resolve;

  return 0;
}

static void
NS(test_main)(void *arg)
{
  int retval;
  int made_pending = 0;

  pending_connection_t *pending_conn = NULL;

  edge_connection_t *exitconn = create_valid_exitconn();
  or_circuit_t *on_circ = tor_malloc_zero(sizeof(or_circuit_t));

  cached_resolve_t *cache_entry = NULL;
  cached_resolve_t query;

  (void)arg;

  TO_CONN(exitconn)->address = tor_strdup("torproject.org");

  strlcpy(query.address, TO_CONN(exitconn)->address, sizeof(query.address));

  NS_MOCK(router_my_exit_policy_is_reject_star);
  NS_MOCK(launch_resolve);

  dns_init();

  retval = dns_resolve_impl(exitconn, 1, on_circ, NULL, &made_pending,
                            NULL);

  tt_int_op(retval,==,0);
  tt_int_op(made_pending,==,1);

  cache_entry = dns_get_cache_entry(&query);

  tt_assert(cache_entry);

  pending_conn = cache_entry->pending_connections;

  tt_assert(pending_conn != NULL);
  tt_assert(pending_conn->conn == exitconn);

  tt_assert(last_launched_resolve == cache_entry);
  tt_str_op(cache_entry->address,==,TO_CONN(exitconn)->address);

  done:
  NS_UNMOCK(router_my_exit_policy_is_reject_star);
  NS_UNMOCK(launch_resolve);
  tor_free(on_circ);
  tor_free(TO_CONN(exitconn)->address);
  if (cache_entry)
    tor_free(cache_entry->pending_connections);
  tor_free(cache_entry);
  tor_free(exitconn);
  return;
}

#undef NS_SUBMODULE

struct testcase_t dns_tests[] = {
   TEST_CASE(clip_ttl),
   TEST_CASE(expiry_ttl),
   TEST_CASE(resolve),
   TEST_CASE_ASPECT(resolve_impl, addr_is_ip_no_need_to_resolve),
   TEST_CASE_ASPECT(resolve_impl, non_exit),
   TEST_CASE_ASPECT(resolve_impl, addr_is_invalid_dest),
   TEST_CASE_ASPECT(resolve_impl, malformed_ptr),
   TEST_CASE_ASPECT(resolve_impl, cache_hit_pending),
   TEST_CASE_ASPECT(resolve_impl, cache_hit_cached),
   TEST_CASE_ASPECT(resolve_impl, cache_miss),
   END_OF_TESTCASES
};

#undef NS_MODULE

