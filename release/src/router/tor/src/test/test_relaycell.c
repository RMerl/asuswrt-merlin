/* Copyright (c) 2014-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Unit tests for handling different kinds of relay cell */

#define RELAY_PRIVATE
#include "or.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "relay.h"
#include "test.h"

static int srm_ncalls;
static entry_connection_t *srm_conn;
static int srm_atype;
static size_t srm_alen;
static int srm_answer_is_set;
static uint8_t srm_answer[512];
static int srm_ttl;
static time_t srm_expires;

/* Mock replacement for connection_ap_hannshake_socks_resolved() */
static void
socks_resolved_mock(entry_connection_t *conn,
                    int answer_type,
                    size_t answer_len,
                    const uint8_t *answer,
                    int ttl,
                    time_t expires)
{
  srm_ncalls++;
  srm_conn = conn;
  srm_atype = answer_type;
  srm_alen = answer_len;
  if (answer) {
    memset(srm_answer, 0, sizeof(srm_answer));
    memcpy(srm_answer, answer, answer_len < 512 ? answer_len : 512);
    srm_answer_is_set = 1;
  } else {
    srm_answer_is_set = 0;
  }
  srm_ttl = ttl;
  srm_expires = expires;
}

static int mum_ncalls;
static entry_connection_t *mum_conn;
static int mum_endreason;

/* Mock replacement for connection_mark_unattached_ap_() */
static void
mark_unattached_mock(entry_connection_t *conn, int endreason,
                     int line, const char *file)
{
  ++mum_ncalls;
  mum_conn = conn;
  mum_endreason = endreason;
  (void) line;
  (void) file;
}

/* Tests for connection_edge_process_resolved_cell().

   The point of ..process_resolved_cell() is to handle an incoming cell
   on an entry connection, and call connection_mark_unattached_ap() and/or
   connection_ap_handshake_socks_resolved().
 */
static void
test_relaycell_resolved(void *arg)
{
  entry_connection_t *entryconn;
  edge_connection_t *edgeconn;
  cell_t cell;
  relay_header_t rh;
  int r;
  or_options_t *options = get_options_mutable();

#define SET_CELL(s) do {                                                \
    memset(&cell, 0, sizeof(cell));                                     \
    memset(&rh, 0, sizeof(rh));                                         \
    memcpy(cell.payload + RELAY_HEADER_SIZE, (s), sizeof((s))-1);       \
    rh.length = sizeof((s))-1;                                          \
    rh.command = RELAY_COMMAND_RESOLVED;                                \
  } while (0)
#define MOCK_RESET() do {                       \
    srm_ncalls = mum_ncalls = 0;                \
  } while (0)
#define ASSERT_MARK_CALLED(reason) do {         \
    tt_int_op(mum_ncalls, OP_EQ, 1);               \
    tt_ptr_op(mum_conn, OP_EQ, entryconn);         \
    tt_int_op(mum_endreason, OP_EQ, (reason));     \
  } while (0)
#define ASSERT_RESOLVED_CALLED(atype, answer, ttl, expires) do {  \
    tt_int_op(srm_ncalls, OP_EQ, 1);                                 \
    tt_ptr_op(srm_conn, OP_EQ, entryconn);                           \
    tt_int_op(srm_atype, OP_EQ, (atype));                            \
    if (answer) {                                                 \
      tt_int_op(srm_alen, OP_EQ, sizeof(answer)-1);                  \
      tt_int_op(srm_alen, OP_LT, 512);                                \
      tt_int_op(srm_answer_is_set, OP_EQ, 1);                        \
      tt_mem_op(srm_answer, OP_EQ, answer, sizeof(answer)-1);        \
    } else {                                                      \
      tt_int_op(srm_answer_is_set, OP_EQ, 0);                        \
    }                                                             \
    tt_int_op(srm_ttl, OP_EQ, ttl);                                  \
    tt_i64_op(srm_expires, OP_EQ, expires);                          \
  } while (0)

  (void)arg;

  MOCK(connection_mark_unattached_ap_, mark_unattached_mock);
  MOCK(connection_ap_handshake_socks_resolved, socks_resolved_mock);

  options->ClientDNSRejectInternalAddresses = 0;

  SET_CELL(/* IPv4: 127.0.1.2, ttl 256 */
           "\x04\x04\x7f\x00\x01\x02\x00\x00\x01\x00"
           /* IPv4: 18.0.0.1, ttl 512 */
           "\x04\x04\x12\x00\x00\x01\x00\x00\x02\x00"
           /* IPv6: 2003::3, ttl 1024 */
           "\x06\x10"
           "\x20\x02\x00\x00\x00\x00\x00\x00"
           "\x00\x00\x00\x00\x00\x00\x00\x03"
           "\x00\x00\x04\x00");

  entryconn = entry_connection_new(CONN_TYPE_AP, AF_INET);
  edgeconn = ENTRY_TO_EDGE_CONN(entryconn);

  /* Try with connection in non-RESOLVE_WAIT state: cell gets ignored */
  MOCK_RESET();
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(srm_ncalls, OP_EQ, 0);
  tt_int_op(mum_ncalls, OP_EQ, 0);

  /* Now put it in the right state. */
  ENTRY_TO_CONN(entryconn)->state = AP_CONN_STATE_RESOLVE_WAIT;
  entryconn->socks_request->command = SOCKS_COMMAND_RESOLVE;
  entryconn->entry_cfg.ipv4_traffic = 1;
  entryconn->entry_cfg.ipv6_traffic = 1;
  entryconn->entry_cfg.prefer_ipv6 = 0;

  /* We prefer ipv4, so we should get the first ipv4 answer */
  MOCK_RESET();
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_IPV4, "\x7f\x00\x01\x02", 256, -1);

  /* But we may be discarding private answers. */
  MOCK_RESET();
  options->ClientDNSRejectInternalAddresses = 1;
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_IPV4, "\x12\x00\x00\x01", 512, -1);

  /* now prefer ipv6, and get the first ipv6 answer */
  entryconn->entry_cfg.prefer_ipv6 = 1;
  MOCK_RESET();
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_IPV6,
                         "\x20\x02\x00\x00\x00\x00\x00\x00"
                         "\x00\x00\x00\x00\x00\x00\x00\x03",
                         1024, -1);

  /* With a cell that only has IPv4, we report IPv4 even if we prefer IPv6 */
  MOCK_RESET();
  SET_CELL("\x04\x04\x12\x00\x00\x01\x00\x00\x02\x00");
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_IPV4, "\x12\x00\x00\x01", 512, -1);

  /* But if we don't allow IPv4, we report nothing if the cell contains only
   * ipv4 */
  MOCK_RESET();
  entryconn->entry_cfg.ipv4_traffic = 0;
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_ERROR, NULL, -1, -1);

  /* If we wanted hostnames, we report nothing, since we only had IPs. */
  MOCK_RESET();
  entryconn->entry_cfg.ipv4_traffic = 1;
  entryconn->socks_request->command = SOCKS_COMMAND_RESOLVE_PTR;
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_ERROR, NULL, -1, -1);

  /* A hostname cell is fine though. */
  MOCK_RESET();
  SET_CELL("\x00\x0fwww.example.com\x00\x01\x00\x00");
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_HOSTNAME, "www.example.com", 65536, -1);

  /* error on malformed cell */
  MOCK_RESET();
  entryconn->socks_request->command = SOCKS_COMMAND_RESOLVE;
  SET_CELL("\x04\x04\x01\x02\x03\x04"); /* no ttl */
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_TORPROTOCOL);
  tt_int_op(srm_ncalls, OP_EQ, 0);

  /* error on all addresses private */
  MOCK_RESET();
  SET_CELL(/* IPv4: 127.0.1.2, ttl 256 */
           "\x04\x04\x7f\x00\x01\x02\x00\x00\x01\x00"
           /* IPv4: 192.168.1.1, ttl 256 */
           "\x04\x04\xc0\xa8\x01\x01\x00\x00\x01\x00");
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_TORPROTOCOL);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_ERROR_TRANSIENT, NULL, 0, TIME_MAX);

  /* Legit error code */
  MOCK_RESET();
  SET_CELL("\xf0\x15" "quiet and meaningless" "\x00\x00\x0f\xff");
  r = connection_edge_process_resolved_cell(edgeconn, &cell, &rh);
  tt_int_op(r, OP_EQ, 0);
  ASSERT_MARK_CALLED(END_STREAM_REASON_DONE|
                     END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
  ASSERT_RESOLVED_CALLED(RESOLVED_TYPE_ERROR_TRANSIENT, NULL, -1, -1);

 done:
  UNMOCK(connection_mark_unattached_ap_);
  UNMOCK(connection_ap_handshake_socks_resolved);
}

struct testcase_t relaycell_tests[] = {
  { "resolved", test_relaycell_resolved, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

