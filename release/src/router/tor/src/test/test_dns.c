#include "or.h"
#include "test.h"

#define DNS_PRIVATE

#include "dns.h"
#include "connection.h"

static void
test_dns_clip_ttl(void *arg)
{
  (void)arg;

  uint32_t ttl_mid = MIN_DNS_TTL / 2 + MAX_DNS_TTL / 2;

  tt_int_op(dns_clip_ttl(MIN_DNS_TTL - 1),==,MIN_DNS_TTL);
  tt_int_op(dns_clip_ttl(ttl_mid),==,ttl_mid);
  tt_int_op(dns_clip_ttl(MAX_DNS_TTL + 1),==,MAX_DNS_TTL);

  done:
  return;
}

static void
test_dns_expiry_ttl(void *arg)
{
  (void)arg;

  uint32_t ttl_mid = MIN_DNS_TTL / 2 + MAX_DNS_ENTRY_AGE / 2;

  tt_int_op(dns_get_expiry_ttl(MIN_DNS_TTL - 1),==,MIN_DNS_TTL);
  tt_int_op(dns_get_expiry_ttl(ttl_mid),==,ttl_mid);
  tt_int_op(dns_get_expiry_ttl(MAX_DNS_ENTRY_AGE + 1),==,MAX_DNS_ENTRY_AGE);

  done:
  return;
}

static int resolve_retval = 0;
static int resolve_made_conn_pending = 0;
static char *resolved_name = NULL;
static cached_resolve_t *cache_entry = NULL;

static int n_fake_impl = 0;

/** This will be our configurable substitute for <b>dns_resolve_impl</b> in
 * dns.c. It will return <b>resolve_retval</b>,
 * and set <b>resolve_made_conn_pending</b> to
 * <b>made_connection_pending_out</b>. It will set <b>hostname_out</b>
 * to a duplicate of <b>resolved_name</b> and it will set <b>resolve_out</b>
 * to <b>cache_entry</b>. Lastly, it will increment <b>n_fake_impl</b< by
 * 1.
 */
static int
dns_resolve_fake_impl(edge_connection_t *exitconn, int is_resolve,
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

  if (resolve_out && cache_entry)
    *resolve_out = cache_entry;

  n_fake_impl++;

  return resolve_retval;
}

static edge_connection_t *conn_for_resolved_cell = NULL;

static int n_send_resolved_cell_replacement = 0;
static uint8_t last_answer_type = 0;
static cached_resolve_t *last_resolved;

static void
send_resolved_cell_replacement(edge_connection_t *conn, uint8_t answer_type,
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
send_resolved_hostname_cell_replacement(edge_connection_t *conn,
                                        const char *hostname)
{
  conn_for_resolved_cell = conn;

  tor_free(last_resolved_hostname);
  last_resolved_hostname = tor_strdup(hostname);

  n_send_resolved_hostname_cell_replacement++;
}

static int n_dns_cancel_pending_resolve_replacement = 0;

static void
dns_cancel_pending_resolve_replacement(const char *address)
{
  (void) address;
  n_dns_cancel_pending_resolve_replacement++;
}

static int n_connection_free = 0;
static connection_t *last_freed_conn = NULL;

static void
connection_free_replacement(connection_t *conn)
{
   n_connection_free++;

   last_freed_conn = conn;
}

static void
test_dns_resolve_outer(void *arg)
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

  MOCK(dns_resolve_impl,dns_resolve_fake_impl);
  MOCK(send_resolved_cell,send_resolved_cell_replacement);
  MOCK(send_resolved_hostname_cell,send_resolved_hostname_cell_replacement);

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

  cache_entry = fake_resolved;

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

  MOCK(dns_cancel_pending_resolve,dns_cancel_pending_resolve_replacement);
  MOCK(connection_free,connection_free_replacement);

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
  UNMOCK(dns_resolve_impl);
  UNMOCK(send_resolved_cell);
  UNMOCK(send_resolved_hostname_cell);
  UNMOCK(dns_cancel_pending_resolve);
  UNMOCK(connection_free);
  tor_free(on_circuit);
  tor_free(exitconn);
  tor_free(nextconn);
  tor_free(resolved_name);
  tor_free(fake_resolved);
  tor_free(last_resolved_hostname);
  return;
}

struct testcase_t dns_tests[] = {
   { "clip_ttl", test_dns_clip_ttl, 0, NULL, NULL },
   { "expiry_ttl", test_dns_expiry_ttl, 0, NULL, NULL },
   { "resolve_outer", test_dns_resolve_outer, TT_FORK, NULL, NULL },
   END_OF_TESTCASES
};

