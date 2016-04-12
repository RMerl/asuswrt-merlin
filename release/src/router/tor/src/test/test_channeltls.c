/* Copyright (c) 2014-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include <math.h>

#define TOR_CHANNEL_INTERNAL_
#include "or.h"
#include "address.h"
#include "buffers.h"
#include "channel.h"
#include "channeltls.h"
#include "connection_or.h"
#include "config.h"
/* For init/free stuff */
#include "scheduler.h"
#include "tortls.h"

/* Test suite stuff */
#include "test.h"
#include "fakechans.h"

/* The channeltls unit tests */
static void test_channeltls_create(void *arg);
static void test_channeltls_num_bytes_queued(void *arg);
static void test_channeltls_overhead_estimate(void *arg);

/* Mocks used by channeltls unit tests */
static size_t tlschan_buf_datalen_mock(const buf_t *buf);
static or_connection_t * tlschan_connection_or_connect_mock(
    const tor_addr_t *addr,
    uint16_t port,
    const char *digest,
    channel_tls_t *tlschan);
static int tlschan_is_local_addr_mock(const tor_addr_t *addr);

/* Fake close method */
static void tlschan_fake_close_method(channel_t *chan);

/* Flags controlling behavior of channeltls unit test mocks */
static int tlschan_local = 0;
static const buf_t * tlschan_buf_datalen_mock_target = NULL;
static size_t tlschan_buf_datalen_mock_size = 0;

/* Thing to cast to fake tor_tls_t * to appease assert_connection_ok() */
static int fake_tortls = 0; /* Bleh... */

static void
test_channeltls_create(void *arg)
{
  tor_addr_t test_addr;
  channel_t *ch = NULL;
  const char test_digest[DIGEST_LEN] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14 };

  (void)arg;

  /* Set up a fake address to fake-connect to */
  test_addr.family = AF_INET;
  test_addr.addr.in_addr.s_addr = htonl(0x01020304);

  /* For this test we always want the address to be treated as non-local */
  tlschan_local = 0;
  /* Install is_local_addr() mock */
  MOCK(is_local_addr, tlschan_is_local_addr_mock);

  /* Install mock for connection_or_connect() */
  MOCK(connection_or_connect, tlschan_connection_or_connect_mock);

  /* Try connecting */
  ch = channel_tls_connect(&test_addr, 567, test_digest);
  tt_assert(ch != NULL);

 done:
  if (ch) {
    MOCK(scheduler_release_channel, scheduler_release_channel_mock);
    /*
     * Use fake close method that doesn't try to do too much to fake
     * orconn
     */
    ch->close = tlschan_fake_close_method;
    channel_mark_for_close(ch);
    free_fake_channel(ch);
    UNMOCK(scheduler_release_channel);
  }

  UNMOCK(connection_or_connect);
  UNMOCK(is_local_addr);

  return;
}

static void
test_channeltls_num_bytes_queued(void *arg)
{
  tor_addr_t test_addr;
  channel_t *ch = NULL;
  const char test_digest[DIGEST_LEN] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14 };
  channel_tls_t *tlschan = NULL;
  size_t len;
  int fake_outbuf = 0, n;

  (void)arg;

  /* Set up a fake address to fake-connect to */
  test_addr.family = AF_INET;
  test_addr.addr.in_addr.s_addr = htonl(0x01020304);

  /* For this test we always want the address to be treated as non-local */
  tlschan_local = 0;
  /* Install is_local_addr() mock */
  MOCK(is_local_addr, tlschan_is_local_addr_mock);

  /* Install mock for connection_or_connect() */
  MOCK(connection_or_connect, tlschan_connection_or_connect_mock);

  /* Try connecting */
  ch = channel_tls_connect(&test_addr, 567, test_digest);
  tt_assert(ch != NULL);

  /*
   * Next, we have to test ch->num_bytes_queued, which is
   * channel_tls_num_bytes_queued_method.  We can't mock
   * connection_get_outbuf_len() directly because it's static INLINE
   * in connection.h, but we can mock buf_datalen().  Note that
   * if bufferevents ever work, this will break with them enabled.
   */

  tt_assert(ch->num_bytes_queued != NULL);
  tlschan = BASE_CHAN_TO_TLS(ch);
  tt_assert(tlschan != NULL);
  if (TO_CONN(tlschan->conn)->outbuf == NULL) {
    /* We need an outbuf to make sure buf_datalen() gets called */
    fake_outbuf = 1;
    TO_CONN(tlschan->conn)->outbuf = buf_new();
  }
  tlschan_buf_datalen_mock_target = TO_CONN(tlschan->conn)->outbuf;
  tlschan_buf_datalen_mock_size = 1024;
  MOCK(buf_datalen, tlschan_buf_datalen_mock);
  len = ch->num_bytes_queued(ch);
  tt_int_op(len, ==, tlschan_buf_datalen_mock_size);
  /*
   * We also cover num_cells_writeable here; since wide_circ_ids = 0 on
   * the fake tlschans, cell_network_size returns 512, and so with
   * tlschan_buf_datalen_mock_size == 1024, we should be able to write
   * ceil((OR_CONN_HIGHWATER - 1024) / 512) = ceil(OR_CONN_HIGHWATER / 512)
   * - 2 cells.
   */
  n = ch->num_cells_writeable(ch);
  tt_int_op(n, ==, CEIL_DIV(OR_CONN_HIGHWATER, 512) - 2);
  UNMOCK(buf_datalen);
  tlschan_buf_datalen_mock_target = NULL;
  tlschan_buf_datalen_mock_size = 0;
  if (fake_outbuf) {
    buf_free(TO_CONN(tlschan->conn)->outbuf);
    TO_CONN(tlschan->conn)->outbuf = NULL;
  }

 done:
  if (ch) {
    MOCK(scheduler_release_channel, scheduler_release_channel_mock);
    /*
     * Use fake close method that doesn't try to do too much to fake
     * orconn
     */
    ch->close = tlschan_fake_close_method;
    channel_mark_for_close(ch);
    free_fake_channel(ch);
    UNMOCK(scheduler_release_channel);
  }

  UNMOCK(connection_or_connect);
  UNMOCK(is_local_addr);

  return;
}

static void
test_channeltls_overhead_estimate(void *arg)
{
  tor_addr_t test_addr;
  channel_t *ch = NULL;
  const char test_digest[DIGEST_LEN] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14 };
  float r;
  channel_tls_t *tlschan = NULL;

  (void)arg;

  /* Set up a fake address to fake-connect to */
  test_addr.family = AF_INET;
  test_addr.addr.in_addr.s_addr = htonl(0x01020304);

  /* For this test we always want the address to be treated as non-local */
  tlschan_local = 0;
  /* Install is_local_addr() mock */
  MOCK(is_local_addr, tlschan_is_local_addr_mock);

  /* Install mock for connection_or_connect() */
  MOCK(connection_or_connect, tlschan_connection_or_connect_mock);

  /* Try connecting */
  ch = channel_tls_connect(&test_addr, 567, test_digest);
  tt_assert(ch != NULL);

  /* First case: silly low ratios should get clamped to 1.0f */
  tlschan = BASE_CHAN_TO_TLS(ch);
  tt_assert(tlschan != NULL);
  tlschan->conn->bytes_xmitted = 128;
  tlschan->conn->bytes_xmitted_by_tls = 64;
  r = ch->get_overhead_estimate(ch);
  tt_assert(fabsf(r - 1.0f) < 1E-12);

  tlschan->conn->bytes_xmitted_by_tls = 127;
  r = ch->get_overhead_estimate(ch);
  tt_assert(fabsf(r - 1.0f) < 1E-12);

  /* Now middle of the range */
  tlschan->conn->bytes_xmitted_by_tls = 192;
  r = ch->get_overhead_estimate(ch);
  tt_assert(fabsf(r - 1.5f) < 1E-12);

  /* Now above the 2.0f clamp */
  tlschan->conn->bytes_xmitted_by_tls = 257;
  r = ch->get_overhead_estimate(ch);
  tt_assert(fabsf(r - 2.0f) < 1E-12);

  tlschan->conn->bytes_xmitted_by_tls = 512;
  r = ch->get_overhead_estimate(ch);
  tt_assert(fabsf(r - 2.0f) < 1E-12);

 done:
  if (ch) {
    MOCK(scheduler_release_channel, scheduler_release_channel_mock);
    /*
     * Use fake close method that doesn't try to do too much to fake
     * orconn
     */
    ch->close = tlschan_fake_close_method;
    channel_mark_for_close(ch);
    free_fake_channel(ch);
    UNMOCK(scheduler_release_channel);
  }

  UNMOCK(connection_or_connect);
  UNMOCK(is_local_addr);

  return;
}

static size_t
tlschan_buf_datalen_mock(const buf_t *buf)
{
  if (buf != NULL && buf == tlschan_buf_datalen_mock_target) {
    return tlschan_buf_datalen_mock_size;
  } else {
    return buf_datalen__real(buf);
  }
}

static or_connection_t *
tlschan_connection_or_connect_mock(const tor_addr_t *addr,
                                   uint16_t port,
                                   const char *digest,
                                   channel_tls_t *tlschan)
{
  or_connection_t *result = NULL;

  tt_assert(addr != NULL);
  tt_assert(port != 0);
  tt_assert(digest != NULL);
  tt_assert(tlschan != NULL);

  /* Make a fake orconn */
  result = tor_malloc_zero(sizeof(*result));
  result->base_.magic = OR_CONNECTION_MAGIC;
  result->base_.state = OR_CONN_STATE_OPEN;
  result->base_.type = CONN_TYPE_OR;
  result->base_.socket_family = addr->family;
  result->base_.address = tor_strdup("<fake>");
  memcpy(&(result->base_.addr), addr, sizeof(tor_addr_t));
  result->base_.port = port;
  memcpy(result->identity_digest, digest, DIGEST_LEN);
  result->chan = tlschan;
  memcpy(&(result->real_addr), addr, sizeof(tor_addr_t));
  result->tls = (tor_tls_t *)((void *)(&fake_tortls));

 done:
  return result;
}

static void
tlschan_fake_close_method(channel_t *chan)
{
  channel_tls_t *tlschan = NULL;

  tt_assert(chan != NULL);
  tt_int_op(chan->magic, ==, TLS_CHAN_MAGIC);

  tlschan = BASE_CHAN_TO_TLS(chan);
  tt_assert(tlschan != NULL);

  /* Just free the fake orconn */
  tor_free(tlschan->conn->base_.address);
  tor_free(tlschan->conn);

  channel_closed(chan);

 done:
  return;
}

static int
tlschan_is_local_addr_mock(const tor_addr_t *addr)
{
  tt_assert(addr != NULL);

 done:
  return tlschan_local;
}

struct testcase_t channeltls_tests[] = {
  { "create", test_channeltls_create, TT_FORK, NULL, NULL },
  { "num_bytes_queued", test_channeltls_num_bytes_queued,
    TT_FORK, NULL, NULL },
  { "overhead_estimate", test_channeltls_overhead_estimate,
    TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

