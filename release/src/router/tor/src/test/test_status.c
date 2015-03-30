#define STATUS_PRIVATE
#define HIBERNATE_PRIVATE
#define LOG_PRIVATE
#define REPHIST_PRIVATE

#include <float.h>
#include <math.h>

#include "or.h"
#include "torlog.h"
#include "tor_queue.h"
#include "status.h"
#include "circuitlist.h"
#include "config.h"
#include "hibernate.h"
#include "rephist.h"
#include "relay.h"
#include "router.h"
#include "main.h"
#include "nodelist.h"
#include "statefile.h"
#include "test.h"

#define NS_MODULE status

#define NS_SUBMODULE count_circuits

/*
 * Test that count_circuits() is correctly counting the number of
 * global circuits.
 */

struct global_circuitlist_s mock_global_circuitlist =
  TOR_LIST_HEAD_INITIALIZER(global_circuitlist);

NS_DECL(struct global_circuitlist_s *, circuit_get_global_list, (void));

static void
NS(test_main)(void *arg)
{
  /* Choose origin_circuit_t wlog. */
  origin_circuit_t *mock_circuit1, *mock_circuit2;
  circuit_t *circ, *tmp;
  int expected_circuits = 2, actual_circuits;

  (void)arg;

  mock_circuit1 = tor_malloc_zero(sizeof(origin_circuit_t));
  mock_circuit2 = tor_malloc_zero(sizeof(origin_circuit_t));
  TOR_LIST_INSERT_HEAD(
    &mock_global_circuitlist, TO_CIRCUIT(mock_circuit1), head);
  TOR_LIST_INSERT_HEAD(
    &mock_global_circuitlist, TO_CIRCUIT(mock_circuit2), head);

  NS_MOCK(circuit_get_global_list);

  actual_circuits = count_circuits();

  tt_assert(expected_circuits == actual_circuits);

  done:
    TOR_LIST_FOREACH_SAFE(
        circ, NS(circuit_get_global_list)(), head, tmp);
      tor_free(circ);
    NS_UNMOCK(circuit_get_global_list);
}

static struct global_circuitlist_s *
NS(circuit_get_global_list)(void)
{
  return &mock_global_circuitlist;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE secs_to_uptime

/*
 * Test that secs_to_uptime() is converting the number of seconds that
 * Tor is up for into the appropriate string form containing hours and minutes.
 */

static void
NS(test_main)(void *arg)
{
  const char *expected;
  char *actual;
  (void)arg;

  expected = "0:00 hours";
  actual = secs_to_uptime(0);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "0:00 hours";
  actual = secs_to_uptime(1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "0:01 hours";
  actual = secs_to_uptime(60);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "0:59 hours";
  actual = secs_to_uptime(60 * 59);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1:00 hours";
  actual = secs_to_uptime(60 * 60);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "23:59 hours";
  actual = secs_to_uptime(60 * 60 * 23 + 60 * 59);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1 day 0:00 hours";
  actual = secs_to_uptime(60 * 60 * 23 + 60 * 60);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1 day 0:00 hours";
  actual = secs_to_uptime(86400 + 1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1 day 0:01 hours";
  actual = secs_to_uptime(86400 + 60);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "10 days 0:00 hours";
  actual = secs_to_uptime(86400 * 10);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "10 days 0:00 hours";
  actual = secs_to_uptime(864000 + 1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "10 days 0:01 hours";
  actual = secs_to_uptime(864000 + 60);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  done:
    if (actual != NULL)
      tor_free(actual);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE bytes_to_usage

/*
 * Test that bytes_to_usage() is correctly converting the number of bytes that
 * Tor has read/written into the appropriate string form containing kilobytes,
 * megabytes, or gigabytes.
 */

static void
NS(test_main)(void *arg)
{
  const char *expected;
  char *actual;
  (void)arg;

  expected = "0 kB";
  actual = bytes_to_usage(0);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "0 kB";
  actual = bytes_to_usage(1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1 kB";
  actual = bytes_to_usage(1024);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1023 kB";
  actual = bytes_to_usage((1 << 20) - 1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.00 MB";
  actual = bytes_to_usage((1 << 20));
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.00 MB";
  actual = bytes_to_usage((1 << 20) + 5242);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.01 MB";
  actual = bytes_to_usage((1 << 20) + 5243);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1024.00 MB";
  actual = bytes_to_usage((1 << 30) - 1);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.00 GB";
  actual = bytes_to_usage((1 << 30));
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.00 GB";
  actual = bytes_to_usage((1 << 30) + 5368709);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "1.01 GB";
  actual = bytes_to_usage((1 << 30) + 5368710);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  expected = "10.00 GB";
  actual = bytes_to_usage((U64_LITERAL(1) << 30) * 10L);
  tt_str_op(actual, ==, expected);
  tor_free(actual);

  done:
    if (actual != NULL)
      tor_free(actual);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, fails)

/*
 * Tests that log_heartbeat() fails when in the public server mode,
 * not hibernating, and we couldn't get the current routerinfo.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(const routerinfo_t *, router_get_my_routerinfo, (void));

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(router_get_my_routerinfo);

  expected = -1;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);

  done:
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(router_get_my_routerinfo);
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 2.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 0;
}

static const or_options_t *
NS(get_options)(void)
{
  return NULL;
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 1;
}

static const routerinfo_t *
NS(router_get_my_routerinfo)(void)
{
  return NULL;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, not_in_consensus)

/*
 * Tests that log_heartbeat() logs appropriately if we are not in the cached
 * consensus.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(const routerinfo_t *, router_get_my_routerinfo, (void));
NS_DECL(const node_t *, node_get_by_id, (const char *identity_digest));
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format, va_list ap));
NS_DECL(int, server_mode, (const or_options_t *options));

static routerinfo_t *mock_routerinfo;
extern int onion_handshakes_requested[MAX_ONION_HANDSHAKE_TYPE+1];
extern int onion_handshakes_assigned[MAX_ONION_HANDSHAKE_TYPE+1];

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(router_get_my_routerinfo);
  NS_MOCK(node_get_by_id);
  NS_MOCK(logv);
  NS_MOCK(server_mode);

  log_global_min_severity_ = LOG_DEBUG;
  onion_handshakes_requested[ONION_HANDSHAKE_TYPE_TAP] = 1;
  onion_handshakes_assigned[ONION_HANDSHAKE_TYPE_TAP] = 1;
  onion_handshakes_requested[ONION_HANDSHAKE_TYPE_NTOR] = 1;
  onion_handshakes_assigned[ONION_HANDSHAKE_TYPE_NTOR] = 1;

  expected = 0;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);
  tt_int_op(CALLED(logv), ==, 3);

  done:
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(router_get_my_routerinfo);
    NS_UNMOCK(node_get_by_id);
    NS_UNMOCK(logv);
    NS_UNMOCK(server_mode);
    tor_free(mock_routerinfo);
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 1.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 0;
}

static const or_options_t *
NS(get_options)(void)
{
  return NULL;
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 1;
}

static const routerinfo_t *
NS(router_get_my_routerinfo)(void)
{
  mock_routerinfo = tor_malloc(sizeof(routerinfo_t));

  return mock_routerinfo;
}

static const node_t *
NS(node_get_by_id)(const char *identity_digest)
{
  (void)identity_digest;

  return NULL;
}

static void
NS(logv)(int severity, log_domain_mask_t domain,
  const char *funcname, const char *suffix, const char *format, va_list ap)
{
  switch (CALLED(logv))
  {
    case 0:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: It seems like we are not in the cached consensus.");
      break;
    case 1:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: Tor's uptime is %s, with %d circuits open. "
          "I've sent %s and received %s.%s");
      tt_str_op(va_arg(ap, char *), ==, "0:00 hours");  /* uptime */
      tt_int_op(va_arg(ap, int), ==, 0);  /* count_circuits() */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_sent */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_rcvd */
      tt_str_op(va_arg(ap, char *), ==, "");  /* hibernating */
      break;
    case 2:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(
          strstr(funcname, "rep_hist_log_circuit_handshake_stats"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
        "Circuit handshake stats since last time: %d/%d TAP, %d/%d NTor.");
      tt_int_op(va_arg(ap, int), ==, 1);  /* handshakes assigned (TAP) */
      tt_int_op(va_arg(ap, int), ==, 1);  /* handshakes requested (TAP) */
      tt_int_op(va_arg(ap, int), ==, 1);  /* handshakes assigned (NTOR) */
      tt_int_op(va_arg(ap, int), ==, 1);  /* handshakes requested (NTOR) */
      break;
    default:
      tt_abort_msg("unexpected call to logv()");  // TODO: prettyprint args
      break;
  }

  done:
    CALLED(logv)++;
}

static int
NS(server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, simple)

/*
 * Tests that log_heartbeat() correctly logs heartbeat information
 * normally.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(long, get_uptime, (void));
NS_DECL(uint64_t, get_bytes_read, (void));
NS_DECL(uint64_t, get_bytes_written, (void));
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format, va_list ap));
NS_DECL(int, server_mode, (const or_options_t *options));

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(get_uptime);
  NS_MOCK(get_bytes_read);
  NS_MOCK(get_bytes_written);
  NS_MOCK(logv);
  NS_MOCK(server_mode);

  log_global_min_severity_ = LOG_DEBUG;

  expected = 0;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);

  done:
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(get_uptime);
    NS_UNMOCK(get_bytes_read);
    NS_UNMOCK(get_bytes_written);
    NS_UNMOCK(logv);
    NS_UNMOCK(server_mode);
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 1.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 1;
}

static const or_options_t *
NS(get_options)(void)
{
  return NULL;
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static long
NS(get_uptime)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_read)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_written)(void)
{
  return 0;
}

static void
NS(logv)(int severity, log_domain_mask_t domain, const char *funcname,
  const char *suffix, const char *format, va_list ap)
{
  tt_int_op(severity, ==, LOG_NOTICE);
  tt_int_op(domain, ==, LD_HEARTBEAT);
  tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
  tt_ptr_op(suffix, ==, NULL);
  tt_str_op(format, ==,
      "Heartbeat: Tor's uptime is %s, with %d circuits open. "
      "I've sent %s and received %s.%s");
  tt_str_op(va_arg(ap, char *), ==, "0:00 hours");  /* uptime */
  tt_int_op(va_arg(ap, int), ==, 0);  /* count_circuits() */
  tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_sent */
  tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_rcvd */
  tt_str_op(va_arg(ap, char *), ==, " We are currently hibernating.");

  done:
    ;
}

static int
NS(server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, calls_log_accounting)

/*
 * Tests that log_heartbeat() correctly logs heartbeat information
 * and accounting information when configured.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(long, get_uptime, (void));
NS_DECL(uint64_t, get_bytes_read, (void));
NS_DECL(uint64_t, get_bytes_written, (void));
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format, va_list ap));
NS_DECL(int, server_mode, (const or_options_t *options));
NS_DECL(or_state_t *, get_or_state, (void));
NS_DECL(int, accounting_is_enabled, (const or_options_t *options));
NS_DECL(time_t, accounting_get_end_time, (void));

static or_state_t * NS(mock_state) = NULL;
static or_options_t * NS(mock_options) = NULL;

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(get_uptime);
  NS_MOCK(get_bytes_read);
  NS_MOCK(get_bytes_written);
  NS_MOCK(logv);
  NS_MOCK(server_mode);
  NS_MOCK(get_or_state);
  NS_MOCK(accounting_is_enabled);
  NS_MOCK(accounting_get_end_time);

  log_global_min_severity_ = LOG_DEBUG;

  expected = 0;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);
  tt_int_op(CALLED(logv), ==, 2);

  done:
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(get_uptime);
    NS_UNMOCK(get_bytes_read);
    NS_UNMOCK(get_bytes_written);
    NS_UNMOCK(logv);
    NS_UNMOCK(server_mode);
    NS_UNMOCK(accounting_is_enabled);
    NS_UNMOCK(accounting_get_end_time);
    tor_free_(NS(mock_state));
    tor_free_(NS(mock_options));
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 1.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 0;
}

static const or_options_t *
NS(get_options)(void)
{
  NS(mock_options) = tor_malloc_zero(sizeof(or_options_t));
  NS(mock_options)->AccountingMax = 0;

  return NS(mock_options);
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static long
NS(get_uptime)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_read)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_written)(void)
{
  return 0;
}

static void
NS(logv)(int severity, log_domain_mask_t domain,
  const char *funcname, const char *suffix, const char *format, va_list ap)
{
  switch (CALLED(logv))
  {
    case 0:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: Tor's uptime is %s, with %d circuits open. "
          "I've sent %s and received %s.%s");
      tt_str_op(va_arg(ap, char *), ==, "0:00 hours");  /* uptime */
      tt_int_op(va_arg(ap, int), ==, 0);  /* count_circuits() */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_sent */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_rcvd */
      tt_str_op(va_arg(ap, char *), ==, "");  /* hibernating */
      break;
    case 1:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_accounting"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: Accounting enabled. Sent: %s / %s, Received: %s / %s. "
          "The current accounting interval ends on %s, in %s.");
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* acc_sent */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* acc_max */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* acc_rcvd */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* acc_max */
      /* format_local_iso_time uses local tz, just check mins and secs. */
      tt_ptr_op(strstr(va_arg(ap, char *), ":01:00"), !=, NULL);  /* end_buf */
      tt_str_op(va_arg(ap, char *), ==, "0:01 hours");   /* remaining */
      break;
    default:
      tt_abort_msg("unexpected call to logv()");  // TODO: prettyprint args
      break;
  }

  done:
    CALLED(logv)++;
}

static int
NS(server_mode)(const or_options_t *options)
{
  (void)options;

  return 1;
}

static int
NS(accounting_is_enabled)(const or_options_t *options)
{
  (void)options;

  return 1;
}

static time_t
NS(accounting_get_end_time)(void)
{
  return 60;
}

static or_state_t *
NS(get_or_state)(void)
{
  NS(mock_state) = tor_malloc_zero(sizeof(or_state_t));
  NS(mock_state)->AccountingBytesReadInInterval = 0;
  NS(mock_state)->AccountingBytesWrittenInInterval = 0;

  return NS(mock_state);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, packaged_cell_fullness)

/*
 * Tests that log_heartbeat() correctly logs packaged cell
 * fullness information.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(long, get_uptime, (void));
NS_DECL(uint64_t, get_bytes_read, (void));
NS_DECL(uint64_t, get_bytes_written, (void));
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format, va_list ap));
NS_DECL(int, server_mode, (const or_options_t *options));
NS_DECL(int, accounting_is_enabled, (const or_options_t *options));

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(get_uptime);
  NS_MOCK(get_bytes_read);
  NS_MOCK(get_bytes_written);
  NS_MOCK(logv);
  NS_MOCK(server_mode);
  NS_MOCK(accounting_is_enabled);
  log_global_min_severity_ = LOG_DEBUG;

  stats_n_data_bytes_packaged = RELAY_PAYLOAD_SIZE;
  stats_n_data_cells_packaged = 1;
  expected = 0;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);
  tt_int_op(CALLED(logv), ==, 2);

  done:
    stats_n_data_bytes_packaged = 0;
    stats_n_data_cells_packaged = 0;
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(get_uptime);
    NS_UNMOCK(get_bytes_read);
    NS_UNMOCK(get_bytes_written);
    NS_UNMOCK(logv);
    NS_UNMOCK(server_mode);
    NS_UNMOCK(accounting_is_enabled);
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 1.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 0;
}

static const or_options_t *
NS(get_options)(void)
{
  return NULL;
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static long
NS(get_uptime)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_read)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_written)(void)
{
  return 0;
}

static void
NS(logv)(int severity, log_domain_mask_t domain, const char *funcname,
    const char *suffix, const char *format, va_list ap)
{
  switch (CALLED(logv))
  {
    case 0:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: Tor's uptime is %s, with %d circuits open. "
          "I've sent %s and received %s.%s");
      tt_str_op(va_arg(ap, char *), ==, "0:00 hours");  /* uptime */
      tt_int_op(va_arg(ap, int), ==, 0);  /* count_circuits() */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_sent */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_rcvd */
      tt_str_op(va_arg(ap, char *), ==, "");  /* hibernating */
      break;
    case 1:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Average packaged cell fullness: %2.3f%%");
      tt_int_op(fabs(va_arg(ap, double) - 100.0) <= DBL_EPSILON, ==, 1);
      break;
    default:
      tt_abort_msg("unexpected call to logv()");  // TODO: prettyprint args
      break;
  }

  done:
    CALLED(logv)++;
}

static int
NS(server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static int
NS(accounting_is_enabled)(const or_options_t *options)
{
  (void)options;

  return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(log_heartbeat, tls_write_overhead)

/*
 * Tests that log_heartbeat() correctly logs the TLS write overhead information
 * when the TLS write overhead ratio exceeds 1.
 */

NS_DECL(double, tls_get_write_overhead_ratio, (void));
NS_DECL(int, we_are_hibernating, (void));
NS_DECL(const or_options_t *, get_options, (void));
NS_DECL(int, public_server_mode, (const or_options_t *options));
NS_DECL(long, get_uptime, (void));
NS_DECL(uint64_t, get_bytes_read, (void));
NS_DECL(uint64_t, get_bytes_written, (void));
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
    const char *funcname, const char *suffix, const char *format, va_list ap));
NS_DECL(int, server_mode, (const or_options_t *options));
NS_DECL(int, accounting_is_enabled, (const or_options_t *options));

static void
NS(test_main)(void *arg)
{
  int expected, actual;
  (void)arg;

  NS_MOCK(tls_get_write_overhead_ratio);
  NS_MOCK(we_are_hibernating);
  NS_MOCK(get_options);
  NS_MOCK(public_server_mode);
  NS_MOCK(get_uptime);
  NS_MOCK(get_bytes_read);
  NS_MOCK(get_bytes_written);
  NS_MOCK(logv);
  NS_MOCK(server_mode);
  NS_MOCK(accounting_is_enabled);
  stats_n_data_cells_packaged = 0;
  log_global_min_severity_ = LOG_DEBUG;

  expected = 0;
  actual = log_heartbeat(0);

  tt_int_op(actual, ==, expected);
  tt_int_op(CALLED(logv), ==, 2);

  done:
    NS_UNMOCK(tls_get_write_overhead_ratio);
    NS_UNMOCK(we_are_hibernating);
    NS_UNMOCK(get_options);
    NS_UNMOCK(public_server_mode);
    NS_UNMOCK(get_uptime);
    NS_UNMOCK(get_bytes_read);
    NS_UNMOCK(get_bytes_written);
    NS_UNMOCK(logv);
    NS_UNMOCK(server_mode);
    NS_UNMOCK(accounting_is_enabled);
}

static double
NS(tls_get_write_overhead_ratio)(void)
{
  return 2.0;
}

static int
NS(we_are_hibernating)(void)
{
  return 0;
}

static const or_options_t *
NS(get_options)(void)
{
  return NULL;
}

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static long
NS(get_uptime)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_read)(void)
{
  return 0;
}

static uint64_t
NS(get_bytes_written)(void)
{
  return 0;
}

static void
NS(logv)(int severity, log_domain_mask_t domain,
  const char *funcname, const char *suffix, const char *format, va_list ap)
{
  switch (CALLED(logv))
  {
    case 0:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==,
          "Heartbeat: Tor's uptime is %s, with %d circuits open. "
          "I've sent %s and received %s.%s");
      tt_str_op(va_arg(ap, char *), ==, "0:00 hours");  /* uptime */
      tt_int_op(va_arg(ap, int), ==, 0);  /* count_circuits() */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_sent */
      tt_str_op(va_arg(ap, char *), ==, "0 kB");  /* bw_rcvd */
      tt_str_op(va_arg(ap, char *), ==, "");  /* hibernating */
      break;
    case 1:
      tt_int_op(severity, ==, LOG_NOTICE);
      tt_int_op(domain, ==, LD_HEARTBEAT);
      tt_ptr_op(strstr(funcname, "log_heartbeat"), !=, NULL);
      tt_ptr_op(suffix, ==, NULL);
      tt_str_op(format, ==, "TLS write overhead: %.f%%");
      tt_int_op(fabs(va_arg(ap, double) - 100.0) <= DBL_EPSILON, ==, 1);
      break;
    default:
      tt_abort_msg("unexpected call to logv()");  // TODO: prettyprint args
      break;
  }

  done:
    CALLED(logv)++;
}

static int
NS(server_mode)(const or_options_t *options)
{
  (void)options;

  return 0;
}

static int
NS(accounting_is_enabled)(const or_options_t *options)
{
  (void)options;

  return 0;
}

#undef NS_SUBMODULE

struct testcase_t status_tests[] = {
  TEST_CASE(count_circuits),
  TEST_CASE(secs_to_uptime),
  TEST_CASE(bytes_to_usage),
  TEST_CASE_ASPECT(log_heartbeat, fails),
  TEST_CASE_ASPECT(log_heartbeat, simple),
  TEST_CASE_ASPECT(log_heartbeat, not_in_consensus),
  TEST_CASE_ASPECT(log_heartbeat, calls_log_accounting),
  TEST_CASE_ASPECT(log_heartbeat, packaged_cell_fullness),
  TEST_CASE_ASPECT(log_heartbeat, tls_write_overhead),
  END_OF_TESTCASES
};

