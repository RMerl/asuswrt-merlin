/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#define CONNECTION_EDGE_PRIVATE
#define RELAY_PRIVATE
#include "or.h"
#include "channel.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "onion.h"
#include "onion_tap.h"
#include "onion_fast.h"
#include "onion_ntor.h"
#include "relay.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void
test_cfmt_relay_header(void *arg)
{
  relay_header_t rh;
  const uint8_t hdr_1[RELAY_HEADER_SIZE] =
    "\x03" "\x00\x00" "\x21\x22" "ABCD" "\x01\x03";
  uint8_t hdr_out[RELAY_HEADER_SIZE];
  (void)arg;

  tt_int_op(sizeof(hdr_1), OP_EQ, RELAY_HEADER_SIZE);
  relay_header_unpack(&rh, hdr_1);
  tt_int_op(rh.command, OP_EQ, 3);
  tt_int_op(rh.recognized, OP_EQ, 0);
  tt_int_op(rh.stream_id, OP_EQ, 0x2122);
  tt_mem_op(rh.integrity, OP_EQ, "ABCD", 4);
  tt_int_op(rh.length, OP_EQ, 0x103);

  relay_header_pack(hdr_out, &rh);
  tt_mem_op(hdr_out, OP_EQ, hdr_1, RELAY_HEADER_SIZE);

 done:
  ;
}

static void
make_relay_cell(cell_t *out, uint8_t command,
                const void *body, size_t bodylen)
{
  relay_header_t rh;

  memset(&rh, 0, sizeof(rh));
  rh.stream_id = 5;
  rh.command = command;
  rh.length = bodylen;

  out->command = CELL_RELAY;
  out->circ_id = 10;
  relay_header_pack(out->payload, &rh);

  memcpy(out->payload + RELAY_HEADER_SIZE, body, bodylen);
}

static void
test_cfmt_begin_cells(void *arg)
{
  cell_t cell;
  begin_cell_t bcell;
  uint8_t end_reason;
  (void)arg;

  /* Try begindir. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN_DIR, "", 0);
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_ptr_op(NULL, OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(0, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(1, OP_EQ, bcell.is_begindir);

  /* A Begindir with extra stuff. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN_DIR, "12345", 5);
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_ptr_op(NULL, OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(0, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(1, OP_EQ, bcell.is_begindir);

  /* A short but valid begin cell */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:9", 6);
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("a.b", OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(9, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* A significantly loner begin cell */
  memset(&bcell, 0x7f, sizeof(bcell));
  {
    const char c[] = "here-is-a-nice-long.hostname.com:65535";
    make_relay_cell(&cell, RELAY_COMMAND_BEGIN, c, strlen(c)+1);
  }
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("here-is-a-nice-long.hostname.com", OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(65535, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* An IPv4 begin cell. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "18.9.22.169:80", 15);
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("18.9.22.169", OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(80, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* An IPv6 begin cell. Let's make sure we handle colons*/
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN,
                  "[2620::6b0:b:1a1a:0:26e5:480e]:80", 34);
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("[2620::6b0:b:1a1a:0:26e5:480e]", OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(80, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* a begin cell with extra junk but not enough for flags. */
  memset(&bcell, 0x7f, sizeof(bcell));
  {
    const char c[] = "another.example.com:80\x00\x01\x02";
    make_relay_cell(&cell, RELAY_COMMAND_BEGIN, c, sizeof(c)-1);
  }
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("another.example.com", OP_EQ, bcell.address);
  tt_int_op(0, OP_EQ, bcell.flags);
  tt_int_op(80, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* a begin cell with flags. */
  memset(&bcell, 0x7f, sizeof(bcell));
  {
    const char c[] = "another.example.com:443\x00\x01\x02\x03\x04";
    make_relay_cell(&cell, RELAY_COMMAND_BEGIN, c, sizeof(c)-1);
  }
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("another.example.com", OP_EQ, bcell.address);
  tt_int_op(0x1020304, OP_EQ, bcell.flags);
  tt_int_op(443, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* a begin cell with flags and even more cruft after that. */
  memset(&bcell, 0x7f, sizeof(bcell));
  {
    const char c[] = "a-further.example.com:22\x00\xee\xaa\x00\xffHi mom";
    make_relay_cell(&cell, RELAY_COMMAND_BEGIN, c, sizeof(c)-1);
  }
  tt_int_op(0, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  tt_str_op("a-further.example.com", OP_EQ, bcell.address);
  tt_int_op(0xeeaa00ff, OP_EQ, bcell.flags);
  tt_int_op(22, OP_EQ, bcell.port);
  tt_int_op(5, OP_EQ, bcell.stream_id);
  tt_int_op(0, OP_EQ, bcell.is_begindir);
  tor_free(bcell.address);

  /* bad begin cell: impossible length. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:80", 7);
  cell.payload[9] = 0x01; /* Set length to 510 */
  cell.payload[10] = 0xfe;
  {
    relay_header_t rh;
    relay_header_unpack(&rh, cell.payload);
    tt_int_op(rh.length, OP_EQ, 510);
  }
  tt_int_op(-2, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* Bad begin cell: no body. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "", 0);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* bad begin cell: no body. */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "", 0);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* bad begin cell: no colon */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b", 4);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* bad begin cell: no ports */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:", 5);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* bad begin cell: bad port */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:xyz", 8);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:100000", 11);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

  /* bad begin cell: no nul */
  memset(&bcell, 0x7f, sizeof(bcell));
  make_relay_cell(&cell, RELAY_COMMAND_BEGIN, "a.b:80", 6);
  tt_int_op(-1, OP_EQ, begin_cell_parse(&cell, &bcell, &end_reason));

 done:
  tor_free(bcell.address);
}

static void
test_cfmt_connected_cells(void *arg)
{
  relay_header_t rh;
  cell_t cell;
  tor_addr_t addr;
  int ttl, r;
  char *mem_op_hex_tmp = NULL;
  (void)arg;

  /* Let's try an oldschool one with nothing in it. */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED, "", 0);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_UNSPEC);
  tt_int_op(ttl, OP_EQ, -1);

  /* A slightly less oldschool one: only an IPv4 address */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED, "\x20\x30\x40\x50", 4);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET);
  tt_str_op(fmt_addr(&addr), OP_EQ, "32.48.64.80");
  tt_int_op(ttl, OP_EQ, -1);

  /* Bogus but understandable: truncated TTL */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED, "\x11\x12\x13\x14\x15", 5);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET);
  tt_str_op(fmt_addr(&addr), OP_EQ, "17.18.19.20");
  tt_int_op(ttl, OP_EQ, -1);

  /* Regular IPv4 one: address and TTL */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x02\x03\x04\x05\x00\x00\x0e\x10", 8);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET);
  tt_str_op(fmt_addr(&addr), OP_EQ, "2.3.4.5");
  tt_int_op(ttl, OP_EQ, 3600);

  /* IPv4 with too-big TTL */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x02\x03\x04\x05\xf0\x00\x00\x00", 8);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET);
  tt_str_op(fmt_addr(&addr), OP_EQ, "2.3.4.5");
  tt_int_op(ttl, OP_EQ, -1);

  /* IPv6 (ttl is mandatory) */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x00\x00\x00\x00\x06"
                  "\x26\x07\xf8\xb0\x40\x0c\x0c\x02"
                  "\x00\x00\x00\x00\x00\x00\x00\x68"
                  "\x00\x00\x02\x58", 25);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET6);
  tt_str_op(fmt_addr(&addr), OP_EQ, "2607:f8b0:400c:c02::68");
  tt_int_op(ttl, OP_EQ, 600);

  /* IPv6 (ttl too big) */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x00\x00\x00\x00\x06"
                  "\x26\x07\xf8\xb0\x40\x0c\x0c\x02"
                  "\x00\x00\x00\x00\x00\x00\x00\x68"
                  "\x90\x00\x02\x58", 25);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET6);
  tt_str_op(fmt_addr(&addr), OP_EQ, "2607:f8b0:400c:c02::68");
  tt_int_op(ttl, OP_EQ, -1);

  /* Bogus size: 3. */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x00\x01\x02", 3);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, -1);

  /* Bogus family: 7. */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x00\x00\x00\x00\x07"
                  "\x26\x07\xf8\xb0\x40\x0c\x0c\x02"
                  "\x00\x00\x00\x00\x00\x00\x00\x68"
                  "\x90\x00\x02\x58", 25);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, -1);

  /* Truncated IPv6. */
  make_relay_cell(&cell, RELAY_COMMAND_CONNECTED,
                  "\x00\x00\x00\x00\x06"
                  "\x26\x07\xf8\xb0\x40\x0c\x0c\x02"
                  "\x00\x00\x00\x00\x00\x00\x00\x68"
                  "\x00\x00\x02", 24);
  relay_header_unpack(&rh, cell.payload);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, -1);

  /* Now make sure we can generate connected cells correctly. */
  /* Try an IPv4 address */
  memset(&rh, 0, sizeof(rh));
  memset(&cell, 0, sizeof(cell));
  tor_addr_parse(&addr, "30.40.50.60");
  rh.length = connected_cell_format_payload(cell.payload+RELAY_HEADER_SIZE,
                                            &addr, 128);
  tt_int_op(rh.length, OP_EQ, 8);
  test_memeq_hex(cell.payload+RELAY_HEADER_SIZE, "1e28323c" "00000080");

  /* Try parsing it. */
  tor_addr_make_unspec(&addr);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET);
  tt_str_op(fmt_addr(&addr), OP_EQ, "30.40.50.60");
  tt_int_op(ttl, OP_EQ, 128);

  /* Try an IPv6 address */
  memset(&rh, 0, sizeof(rh));
  memset(&cell, 0, sizeof(cell));
  tor_addr_parse(&addr, "2620::6b0:b:1a1a:0:26e5:480e");
  rh.length = connected_cell_format_payload(cell.payload+RELAY_HEADER_SIZE,
                                            &addr, 3600);
  tt_int_op(rh.length, OP_EQ, 25);
  test_memeq_hex(cell.payload + RELAY_HEADER_SIZE,
                 "00000000" "06"
                 "2620000006b0000b1a1a000026e5480e" "00000e10");

  /* Try parsing it. */
  tor_addr_make_unspec(&addr);
  r = connected_cell_parse(&rh, &cell, &addr, &ttl);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(tor_addr_family(&addr), OP_EQ, AF_INET6);
  tt_str_op(fmt_addr(&addr), OP_EQ, "2620:0:6b0:b:1a1a:0:26e5:480e");
  tt_int_op(ttl, OP_EQ, 3600);

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_cfmt_create_cells(void *arg)
{
  uint8_t b[MAX_ONIONSKIN_CHALLENGE_LEN];
  create_cell_t cc;
  cell_t cell;
  cell_t cell2;

  (void)arg;

  /* === Let's try parsing some good cells! */

  /* A valid create cell. */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, TAP_ONIONSKIN_CHALLENGE_LEN);
  cell.command = CELL_CREATE;
  memcpy(cell.payload, b, TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_int_op(0, OP_EQ, create_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATE, OP_EQ, cc.cell_type);
  tt_int_op(ONION_HANDSHAKE_TYPE_TAP, OP_EQ, cc.handshake_type);
  tt_int_op(TAP_ONIONSKIN_CHALLENGE_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.onionskin,OP_EQ, b, TAP_ONIONSKIN_CHALLENGE_LEN + 10);
  tt_int_op(0, OP_EQ, create_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A valid create_fast cell. */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, CREATE_FAST_LEN);
  cell.command = CELL_CREATE_FAST;
  memcpy(cell.payload, b, CREATE_FAST_LEN);
  tt_int_op(0, OP_EQ, create_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATE_FAST, OP_EQ, cc.cell_type);
  tt_int_op(ONION_HANDSHAKE_TYPE_FAST, OP_EQ, cc.handshake_type);
  tt_int_op(CREATE_FAST_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.onionskin,OP_EQ, b, CREATE_FAST_LEN + 10);
  tt_int_op(0, OP_EQ, create_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A valid create2 cell with a TAP payload */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, TAP_ONIONSKIN_CHALLENGE_LEN);
  cell.command = CELL_CREATE2;
  memcpy(cell.payload, "\x00\x00\x00\xBA", 4); /* TAP, 186 bytes long */
  memcpy(cell.payload+4, b, TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_int_op(0, OP_EQ, create_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATE2, OP_EQ, cc.cell_type);
  tt_int_op(ONION_HANDSHAKE_TYPE_TAP, OP_EQ, cc.handshake_type);
  tt_int_op(TAP_ONIONSKIN_CHALLENGE_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.onionskin,OP_EQ, b, TAP_ONIONSKIN_CHALLENGE_LEN + 10);
  tt_int_op(0, OP_EQ, create_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A valid create2 cell with an ntor payload */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, NTOR_ONIONSKIN_LEN);
  cell.command = CELL_CREATE2;
  memcpy(cell.payload, "\x00\x02\x00\x54", 4); /* ntor, 84 bytes long */
  memcpy(cell.payload+4, b, NTOR_ONIONSKIN_LEN);
  tt_int_op(0, OP_EQ, create_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATE2, OP_EQ, cc.cell_type);
  tt_int_op(ONION_HANDSHAKE_TYPE_NTOR, OP_EQ, cc.handshake_type);
  tt_int_op(NTOR_ONIONSKIN_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.onionskin,OP_EQ, b, NTOR_ONIONSKIN_LEN + 10);
  tt_int_op(0, OP_EQ, create_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A valid create cell with an ntor payload, in legacy format. */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, NTOR_ONIONSKIN_LEN);
  cell.command = CELL_CREATE;
  memcpy(cell.payload, "ntorNTORntorNTOR", 16);
  memcpy(cell.payload+16, b, NTOR_ONIONSKIN_LEN);
  tt_int_op(0, OP_EQ, create_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATE, OP_EQ, cc.cell_type);
  tt_int_op(ONION_HANDSHAKE_TYPE_NTOR, OP_EQ, cc.handshake_type);
  tt_int_op(NTOR_ONIONSKIN_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.onionskin,OP_EQ, b, NTOR_ONIONSKIN_LEN + 10);
  tt_int_op(0, OP_EQ, create_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* == Okay, now let's try to parse some impossible stuff. */

  /* It has to be some kind of a create cell! */
  cell.command = CELL_CREATED;
  tt_int_op(-1, OP_EQ, create_cell_parse(&cc, &cell));

  /* You can't acutally make an unparseable CREATE or CREATE_FAST cell. */

  /* Try some CREATE2 cells.  First with a bad type. */
  cell.command = CELL_CREATE2;
  memcpy(cell.payload, "\x00\x50\x00\x99", 4); /* Type 0x50???? */
  tt_int_op(-1, OP_EQ, create_cell_parse(&cc, &cell));
  /* Now a good type with an incorrect length. */
  memcpy(cell.payload, "\x00\x00\x00\xBC", 4); /* TAP, 187 bytes.*/
  tt_int_op(-1, OP_EQ, create_cell_parse(&cc, &cell));
  /* Now a good type with a ridiculous length. */
  memcpy(cell.payload, "\x00\x00\x02\x00", 4); /* TAP, 512 bytes.*/
  tt_int_op(-1, OP_EQ, create_cell_parse(&cc, &cell));

  /* == Time to try formatting bad cells.  The important thing is that
     we reject big lengths, so just check that for now. */
  cc.handshake_len = 512;
  tt_int_op(-1, OP_EQ, create_cell_format(&cell2, &cc));

  /* == Try formatting a create2 cell we don't understand. XXXX */

 done:
  ;
}

static void
test_cfmt_created_cells(void *arg)
{
  uint8_t b[512];
  created_cell_t cc;
  cell_t cell;
  cell_t cell2;

  (void)arg;

  /* A good CREATED cell */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, TAP_ONIONSKIN_REPLY_LEN);
  cell.command = CELL_CREATED;
  memcpy(cell.payload, b, TAP_ONIONSKIN_REPLY_LEN);
  tt_int_op(0, OP_EQ, created_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATED, OP_EQ, cc.cell_type);
  tt_int_op(TAP_ONIONSKIN_REPLY_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.reply,OP_EQ, b, TAP_ONIONSKIN_REPLY_LEN + 10);
  tt_int_op(0, OP_EQ, created_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A good CREATED_FAST cell */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, CREATED_FAST_LEN);
  cell.command = CELL_CREATED_FAST;
  memcpy(cell.payload, b, CREATED_FAST_LEN);
  tt_int_op(0, OP_EQ, created_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATED_FAST, OP_EQ, cc.cell_type);
  tt_int_op(CREATED_FAST_LEN, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.reply,OP_EQ, b, CREATED_FAST_LEN + 10);
  tt_int_op(0, OP_EQ, created_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A good CREATED2 cell with short reply */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, 64);
  cell.command = CELL_CREATED2;
  memcpy(cell.payload, "\x00\x40", 2);
  memcpy(cell.payload+2, b, 64);
  tt_int_op(0, OP_EQ, created_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATED2, OP_EQ, cc.cell_type);
  tt_int_op(64, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.reply,OP_EQ, b, 80);
  tt_int_op(0, OP_EQ, created_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* A good CREATED2 cell with maximal reply */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, 496);
  cell.command = CELL_CREATED2;
  memcpy(cell.payload, "\x01\xF0", 2);
  memcpy(cell.payload+2, b, 496);
  tt_int_op(0, OP_EQ, created_cell_parse(&cc, &cell));
  tt_int_op(CELL_CREATED2, OP_EQ, cc.cell_type);
  tt_int_op(496, OP_EQ, cc.handshake_len);
  tt_mem_op(cc.reply,OP_EQ, b, 496);
  tt_int_op(0, OP_EQ, created_cell_format(&cell2, &cc));
  tt_int_op(cell.command, OP_EQ, cell2.command);
  tt_mem_op(cell.payload,OP_EQ, cell2.payload, CELL_PAYLOAD_SIZE);

  /* Bogus CREATED2 cell: too long! */
  memset(&cell, 0, sizeof(cell));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, 496);
  cell.command = CELL_CREATED2;
  memcpy(cell.payload, "\x01\xF1", 2);
  tt_int_op(-1, OP_EQ, created_cell_parse(&cc, &cell));

  /* Unformattable CREATED2 cell: too long! */
  cc.handshake_len = 497;
  tt_int_op(-1, OP_EQ, created_cell_format(&cell2, &cc));

 done:
  ;
}

static void
test_cfmt_extend_cells(void *arg)
{
  cell_t cell;
  uint8_t b[512];
  extend_cell_t ec;
  create_cell_t *cc = &ec.create_cell;
  uint8_t p[RELAY_PAYLOAD_SIZE];
  uint8_t p2[RELAY_PAYLOAD_SIZE];
  uint8_t p2_cmd;
  uint16_t p2_len;
  char *mem_op_hex_tmp = NULL;

  (void) arg;

  /* Let's start with a simple EXTEND cell. */
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, TAP_ONIONSKIN_CHALLENGE_LEN);
  memcpy(p, "\x12\xf4\x00\x01\x01\x02", 6); /* 18 244 0 1 : 258 */
  memcpy(p+6,b,TAP_ONIONSKIN_CHALLENGE_LEN);
  memcpy(p+6+TAP_ONIONSKIN_CHALLENGE_LEN, "electroencephalogram", 20);
  tt_int_op(0, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND,
                                     p, 26+TAP_ONIONSKIN_CHALLENGE_LEN));
  tt_int_op(RELAY_COMMAND_EXTEND, OP_EQ, ec.cell_type);
  tt_str_op("18.244.0.1", OP_EQ, fmt_addr(&ec.orport_ipv4.addr));
  tt_int_op(258, OP_EQ, ec.orport_ipv4.port);
  tt_int_op(AF_UNSPEC, OP_EQ, tor_addr_family(&ec.orport_ipv6.addr));
  tt_mem_op(ec.node_id,OP_EQ, "electroencephalogram", 20);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATE);
  tt_int_op(cc->handshake_type, OP_EQ, ONION_HANDSHAKE_TYPE_TAP);
  tt_int_op(cc->handshake_len, OP_EQ, TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_mem_op(cc->onionskin,OP_EQ, b, TAP_ONIONSKIN_CHALLENGE_LEN+20);
  tt_int_op(0, OP_EQ, extend_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(p2_cmd, OP_EQ, RELAY_COMMAND_EXTEND);
  tt_int_op(p2_len, OP_EQ, 26+TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_mem_op(p2,OP_EQ, p, RELAY_PAYLOAD_SIZE);

  /* Let's do an ntor stuffed in a legacy EXTEND cell */
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, NTOR_ONIONSKIN_LEN);
  memcpy(p, "\x12\xf4\x00\x01\x01\x02", 6); /* 18 244 0 1 : 258 */
  memcpy(p+6,"ntorNTORntorNTOR", 16);
  memcpy(p+22, b, NTOR_ONIONSKIN_LEN);
  memcpy(p+6+TAP_ONIONSKIN_CHALLENGE_LEN, "electroencephalogram", 20);
  tt_int_op(0, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND,
                                     p, 26+TAP_ONIONSKIN_CHALLENGE_LEN));
  tt_int_op(RELAY_COMMAND_EXTEND, OP_EQ, ec.cell_type);
  tt_str_op("18.244.0.1", OP_EQ, fmt_addr(&ec.orport_ipv4.addr));
  tt_int_op(258, OP_EQ, ec.orport_ipv4.port);
  tt_int_op(AF_UNSPEC, OP_EQ, tor_addr_family(&ec.orport_ipv6.addr));
  tt_mem_op(ec.node_id,OP_EQ, "electroencephalogram", 20);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATE2);
  tt_int_op(cc->handshake_type, OP_EQ, ONION_HANDSHAKE_TYPE_NTOR);
  tt_int_op(cc->handshake_len, OP_EQ, NTOR_ONIONSKIN_LEN);
  tt_mem_op(cc->onionskin,OP_EQ, b, NTOR_ONIONSKIN_LEN+20);
  tt_int_op(0, OP_EQ, extend_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(p2_cmd, OP_EQ, RELAY_COMMAND_EXTEND);
  tt_int_op(p2_len, OP_EQ, 26+TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_mem_op(p2,OP_EQ, p, RELAY_PAYLOAD_SIZE);
  tt_int_op(0, OP_EQ, create_cell_format_relayed(&cell, cc));

  /* Now let's do a minimal ntor EXTEND2 cell. */
  memset(&ec, 0xff, sizeof(ec));
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, NTOR_ONIONSKIN_LEN);
  /* 2 items; one 18.244.0.1:61681 */
  memcpy(p, "\x02\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  /* The other is a digest. */
  memcpy(p+9, "\x02\x14" "anarchoindividualist", 22);
  /* Prep for the handshake: type and length */
  memcpy(p+31, "\x00\x02\x00\x54", 4);
  memcpy(p+35, b, NTOR_ONIONSKIN_LEN);
  tt_int_op(0, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                     p, 35+NTOR_ONIONSKIN_LEN));
  tt_int_op(RELAY_COMMAND_EXTEND2, OP_EQ, ec.cell_type);
  tt_str_op("18.244.0.1", OP_EQ, fmt_addr(&ec.orport_ipv4.addr));
  tt_int_op(61681, OP_EQ, ec.orport_ipv4.port);
  tt_int_op(AF_UNSPEC, OP_EQ, tor_addr_family(&ec.orport_ipv6.addr));
  tt_mem_op(ec.node_id,OP_EQ, "anarchoindividualist", 20);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATE2);
  tt_int_op(cc->handshake_type, OP_EQ, ONION_HANDSHAKE_TYPE_NTOR);
  tt_int_op(cc->handshake_len, OP_EQ, NTOR_ONIONSKIN_LEN);
  tt_mem_op(cc->onionskin,OP_EQ, b, NTOR_ONIONSKIN_LEN+20);
  tt_int_op(0, OP_EQ, extend_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(p2_cmd, OP_EQ, RELAY_COMMAND_EXTEND2);
  tt_int_op(p2_len, OP_EQ, 35+NTOR_ONIONSKIN_LEN);
  tt_mem_op(p2,OP_EQ, p, RELAY_PAYLOAD_SIZE);

  /* Now let's do a fanciful EXTEND2 cell. */
  memset(&ec, 0xff, sizeof(ec));
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, 99);
  /* 4 items; one 18 244 0 1 61681 */
  memcpy(p, "\x04\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  /* One is a digest. */
  memcpy(p+9, "\x02\x14" "anthropomorphization", 22);
  /* One is an ipv6 address */
  memcpy(p+31, "\x01\x12\x20\x02\x00\x00\x00\x00\x00\x00"
               "\x00\x00\x00\x00\x00\xf0\xc5\x1e\x11\x12", 20);
  /* One is the Konami code. */
  memcpy(p+51, "\xf0\x20upupdowndownleftrightleftrightba", 34);
  /* Prep for the handshake: weird type and length */
  memcpy(p+85, "\x01\x05\x00\x63", 4);
  memcpy(p+89, b, 99);
  tt_int_op(0, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2, p, 89+99));
  tt_int_op(RELAY_COMMAND_EXTEND2, OP_EQ, ec.cell_type);
  tt_str_op("18.244.0.1", OP_EQ, fmt_addr(&ec.orport_ipv4.addr));
  tt_int_op(61681, OP_EQ, ec.orport_ipv4.port);
  tt_str_op("2002::f0:c51e", OP_EQ, fmt_addr(&ec.orport_ipv6.addr));
  tt_int_op(4370, OP_EQ, ec.orport_ipv6.port);
  tt_mem_op(ec.node_id,OP_EQ, "anthropomorphization", 20);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATE2);
  tt_int_op(cc->handshake_type, OP_EQ, 0x105);
  tt_int_op(cc->handshake_len, OP_EQ, 99);
  tt_mem_op(cc->onionskin,OP_EQ, b, 99+20);
  tt_int_op(0, OP_EQ, extend_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(p2_cmd, OP_EQ, RELAY_COMMAND_EXTEND2);
  /* We'll generate it minus the IPv6 address and minus the konami code */
  tt_int_op(p2_len, OP_EQ, 89+99-34-20);
  test_memeq_hex(p2,
                 /* Two items: one that same darn IP address. */
                 "02000612F40001F0F1"
                 /* The next is a digest : anthropomorphization */
                 "0214616e7468726f706f6d6f727068697a6174696f6e"
                 /* Now the handshake prologue */
                 "01050063");
  tt_mem_op(p2+1+8+22+4,OP_EQ, b, 99+20);
  tt_int_op(0, OP_EQ, create_cell_format_relayed(&cell, cc));

  /* == Now try parsing some junk */

  /* Try a too-long handshake */
  memset(p, 0, sizeof(p));
  memcpy(p, "\x02\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  memcpy(p+9, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+31, "\xff\xff\x01\xd0", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* Try two identities. */
  memset(p, 0, sizeof(p));
  memcpy(p, "\x03\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  memcpy(p+9, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+31, "\x02\x14" "autodepolymerization", 22);
  memcpy(p+53, "\xff\xff\x00\x10", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* No identities. */
  memset(p, 0, sizeof(p));
  memcpy(p, "\x01\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  memcpy(p+53, "\xff\xff\x00\x10", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* Try a bad IPv4 address (too long, too short)*/
  memset(p, 0, sizeof(p));
  memcpy(p, "\x02\x00\x07\x12\xf4\x00\x01\xf0\xf1\xff", 10);
  memcpy(p+10, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+32, "\xff\xff\x00\x10", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));
  memset(p, 0, sizeof(p));
  memcpy(p, "\x02\x00\x05\x12\xf4\x00\x01\xf0", 8);
  memcpy(p+8, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+30, "\xff\xff\x00\x10", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* IPv6 address (too long, too short, no IPv4)*/
  memset(p, 0, sizeof(p));
  memcpy(p, "\x03\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  memcpy(p+9, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+31, "\x01\x13" "xxxxxxxxxxxxxxxxYYZ", 19);
  memcpy(p+50, "\xff\xff\x00\x20", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));
  memset(p, 0, sizeof(p));
  memcpy(p, "\x03\x00\x06\x12\xf4\x00\x01\xf0\xf1", 9);
  memcpy(p+9, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+31, "\x01\x11" "xxxxxxxxxxxxxxxxY", 17);
  memcpy(p+48, "\xff\xff\x00\x20", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));
  memset(p, 0, sizeof(p));
  memcpy(p, "\x02", 1);
  memcpy(p+1, "\x02\x14" "anarchoindividualist", 22);
  memcpy(p+23, "\x01\x12" "xxxxxxxxxxxxxxxxYY", 18);
  memcpy(p+41, "\xff\xff\x00\x20", 4);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* Running out of space in specifiers  */
  memset(p,0,sizeof(p));
  memcpy(p, "\x05\x0a\xff", 3);
  memcpy(p+3+255, "\x0a\xff", 2);
  tt_int_op(-1, OP_EQ, extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2,
                                      p, sizeof(p)));

  /* Fuzz, because why not. */
  memset(&ec, 0xff, sizeof(ec));
  {
    int i;
    memset(p, 0, sizeof(p));
    for (i = 0; i < 10000; ++i) {
      int n = crypto_rand_int(sizeof(p));
      crypto_rand((char *)p, n);
      extend_cell_parse(&ec, RELAY_COMMAND_EXTEND2, p, n);
    }
  }

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_cfmt_extended_cells(void *arg)
{
  uint8_t b[512];
  extended_cell_t ec;
  created_cell_t *cc = &ec.created_cell;
  uint8_t p[RELAY_PAYLOAD_SIZE];
  uint8_t p2[RELAY_PAYLOAD_SIZE];
  uint8_t p2_cmd;
  uint16_t p2_len;
  char *mem_op_hex_tmp = NULL;

  (void) arg;

  /* Try a regular EXTENDED cell. */
  memset(&ec, 0xff, sizeof(ec));
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, TAP_ONIONSKIN_REPLY_LEN);
  memcpy(p,b,TAP_ONIONSKIN_REPLY_LEN);
  tt_int_op(0, OP_EQ, extended_cell_parse(&ec, RELAY_COMMAND_EXTENDED, p,
                                       TAP_ONIONSKIN_REPLY_LEN));
  tt_int_op(RELAY_COMMAND_EXTENDED, OP_EQ, ec.cell_type);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATED);
  tt_int_op(cc->handshake_len, OP_EQ, TAP_ONIONSKIN_REPLY_LEN);
  tt_mem_op(cc->reply,OP_EQ, b, TAP_ONIONSKIN_REPLY_LEN);
  tt_int_op(0, OP_EQ, extended_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(RELAY_COMMAND_EXTENDED, OP_EQ, p2_cmd);
  tt_int_op(TAP_ONIONSKIN_REPLY_LEN, OP_EQ, p2_len);
  tt_mem_op(p2,OP_EQ, p, sizeof(p2));

  /* Try an EXTENDED2 cell */
  memset(&ec, 0xff, sizeof(ec));
  memset(p, 0, sizeof(p));
  memset(b, 0, sizeof(b));
  crypto_rand((char*)b, 42);
  memcpy(p,"\x00\x2a",2);
  memcpy(p+2,b,42);
  tt_int_op(0, OP_EQ,
            extended_cell_parse(&ec, RELAY_COMMAND_EXTENDED2, p, 2+42));
  tt_int_op(RELAY_COMMAND_EXTENDED2, OP_EQ, ec.cell_type);
  tt_int_op(cc->cell_type, OP_EQ, CELL_CREATED2);
  tt_int_op(cc->handshake_len, OP_EQ, 42);
  tt_mem_op(cc->reply,OP_EQ, b, 42+10);
  tt_int_op(0, OP_EQ, extended_cell_format(&p2_cmd, &p2_len, p2, &ec));
  tt_int_op(RELAY_COMMAND_EXTENDED2, OP_EQ, p2_cmd);
  tt_int_op(2+42, OP_EQ, p2_len);
  tt_mem_op(p2,OP_EQ, p, sizeof(p2));

  /* Try an almost-too-long EXTENDED2 cell */
  memcpy(p, "\x01\xf0", 2);
  tt_int_op(0, OP_EQ,
            extended_cell_parse(&ec, RELAY_COMMAND_EXTENDED2, p, sizeof(p)));

  /* Now try a too-long extended2 cell. That's the only misparse I can think
   * of. */
  memcpy(p, "\x01\xf1", 2);
  tt_int_op(-1, OP_EQ,
            extended_cell_parse(&ec, RELAY_COMMAND_EXTENDED2, p, sizeof(p)));

 done:
  tor_free(mem_op_hex_tmp);
}

static void
test_cfmt_resolved_cells(void *arg)
{
  smartlist_t *addrs = smartlist_new();
  relay_header_t rh;
  cell_t cell;
  int r, errcode;
  address_ttl_t *a;

  (void)arg;
#define CLEAR_CELL() do {                       \
    memset(&cell, 0, sizeof(cell));             \
    memset(&rh, 0, sizeof(rh));                 \
  } while (0)
#define CLEAR_ADDRS() do {                              \
    SMARTLIST_FOREACH(addrs, address_ttl_t *, a,        \
                      address_ttl_free(a); );           \
    smartlist_clear(addrs);                             \
  } while (0)
#define SET_CELL(s) do {                                                \
    CLEAR_CELL();                                                       \
    memcpy(cell.payload + RELAY_HEADER_SIZE, (s), sizeof((s))-1);       \
    rh.length = sizeof((s))-1;                                          \
    rh.command = RELAY_COMMAND_RESOLVED;                                \
    errcode = -1;                                                       \
  } while (0)

  /* The cell format is one or more answers; each of the form
   *  type [1 byte---0:hostname, 4:ipv4, 6:ipv6, f0:err-transient, f1:err]
   *  length [1 byte]
   *  body [length bytes]
   *  ttl  [4 bytes]
   */

  /* Let's try an empty cell */
  SET_CELL("");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  CLEAR_ADDRS(); /* redundant but let's be consistent */

  /* Cell with one ipv4 addr */
  SET_CELL("\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00");
  tt_int_op(rh.length, OP_EQ, 10);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 1);
  a = smartlist_get(addrs, 0);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "127.0.2.10");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 256);
  CLEAR_ADDRS();

  /* Cell with one ipv6 addr */
  SET_CELL("\x06\x10"
           "\x20\x02\x90\x90\x00\x00\x00\x00"
           "\x00\x00\x00\x00\xf0\xf0\xab\xcd"
           "\x02\00\x00\x01");
  tt_int_op(rh.length, OP_EQ, 22);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 1);
  a = smartlist_get(addrs, 0);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "2002:9090::f0f0:abcd");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 0x2000001);
  CLEAR_ADDRS();

  /* Cell with one hostname */
  SET_CELL("\x00\x11"
           "motherbrain.zebes"
           "\x00\00\x00\x00");
  tt_int_op(rh.length, OP_EQ, 23);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 1);
  a = smartlist_get(addrs, 0);
  tt_assert(tor_addr_is_null(&a->addr));
  tt_str_op(a->hostname, OP_EQ, "motherbrain.zebes");
  tt_int_op(a->ttl, OP_EQ, 0);
  CLEAR_ADDRS();

#define LONG_NAME \
  "this-hostname-has-255-characters.in-order-to-test-whether-very-long.ho" \
  "stnames-are-accepted.i-am-putting-it-in-a-macro-because-although.this-" \
  "function-is-already-very-full.of-copy-and-pasted-stuff.having-this-app" \
  "ear-more-than-once-would-bother-me-somehow.is"

  tt_int_op(strlen(LONG_NAME), OP_EQ, 255);
  SET_CELL("\x00\xff"
           LONG_NAME
           "\x00\01\x00\x00");
  tt_int_op(rh.length, OP_EQ, 261);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 1);
  a = smartlist_get(addrs, 0);
  tt_assert(tor_addr_is_null(&a->addr));
  tt_str_op(a->hostname, OP_EQ, LONG_NAME);
  tt_int_op(a->ttl, OP_EQ, 65536);
  CLEAR_ADDRS();

  /* Cells with an error */
  SET_CELL("\xf0\x2b"
           "I'm sorry, Dave. I'm afraid I can't do that"
           "\x00\x11\x22\x33");
  tt_int_op(rh.length, OP_EQ, 49);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, RESOLVED_TYPE_ERROR_TRANSIENT);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  CLEAR_ADDRS();

  SET_CELL("\xf1\x40"
           "This hostname is too important for me to allow you to resolve it"
           "\x00\x00\x00\x00");
  tt_int_op(rh.length, OP_EQ, 70);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, RESOLVED_TYPE_ERROR);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  CLEAR_ADDRS();

  /* Cell with an unrecognized type */
  SET_CELL("\xee\x16"
           "fault in the AE35 unit"
           "\x09\x09\x01\x01");
  tt_int_op(rh.length, OP_EQ, 28);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  CLEAR_ADDRS();

  /* Cell with one of each */
  SET_CELL(/* unrecognized: */
           "\xee\x16"
           "fault in the AE35 unit"
           "\x09\x09\x01\x01"
           /* error: */
           "\xf0\x2b"
           "I'm sorry, Dave. I'm afraid I can't do that"
           "\x00\x11\x22\x33"
           /* IPv6: */
           "\x06\x10"
           "\x20\x02\x90\x90\x00\x00\x00\x00"
           "\x00\x00\x00\x00\xf0\xf0\xab\xcd"
           "\x02\00\x00\x01"
           /* IPv4: */
           "\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00"
           /* Hostname: */
           "\x00\x11"
           "motherbrain.zebes"
           "\x00\00\x00\x00"
           );
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0); /* no error reported; we got answers */
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 3);
  a = smartlist_get(addrs, 0);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "2002:9090::f0f0:abcd");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 0x2000001);
  a = smartlist_get(addrs, 1);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "127.0.2.10");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 256);
  a = smartlist_get(addrs, 2);
  tt_assert(tor_addr_is_null(&a->addr));
  tt_str_op(a->hostname, OP_EQ, "motherbrain.zebes");
  tt_int_op(a->ttl, OP_EQ, 0);
  CLEAR_ADDRS();

  /* Cell with several of similar type */
  SET_CELL(/* IPv4 */
           "\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00"
           "\x04\x04" "\x08\x08\x08\x08" "\x00\00\x01\x05"
           "\x04\x04" "\x7f\xb0\x02\xb0" "\x00\01\xff\xff"
           /* IPv6 */
           "\x06\x10"
           "\x20\x02\x90\x00\x00\x00\x00\x00"
           "\x00\x00\x00\x00\xca\xfe\xf0\x0d"
           "\x00\00\x00\x01"
           "\x06\x10"
           "\x20\x02\x90\x01\x00\x00\x00\x00"
           "\x00\x00\x00\x00\x00\xfa\xca\xde"
           "\x00\00\x00\x03");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 5);
  a = smartlist_get(addrs, 0);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "127.0.2.10");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 256);
  a = smartlist_get(addrs, 1);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "8.8.8.8");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 261);
  a = smartlist_get(addrs, 2);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "127.176.2.176");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 131071);
  a = smartlist_get(addrs, 3);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "2002:9000::cafe:f00d");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 1);
  a = smartlist_get(addrs, 4);
  tt_str_op(fmt_addr(&a->addr), OP_EQ, "2002:9001::fa:cade");
  tt_ptr_op(a->hostname, OP_EQ, NULL);
  tt_int_op(a->ttl, OP_EQ, 3);
  CLEAR_ADDRS();

  /* Full cell */
#define LONG_NAME2 \
  "this-name-has-231-characters.so-that-it-plus-LONG_NAME-can-completely-" \
  "fill-up-the-payload-of-a-cell.its-important-to-check-for-the-full-thin" \
  "g-case.to-avoid-off-by-one-errors.where-full-things-are-misreported-as" \
  ".overflowing-by-one.z"

  tt_int_op(strlen(LONG_NAME2), OP_EQ, 231);
  SET_CELL("\x00\xff"
           LONG_NAME
           "\x00\01\x00\x00"
           "\x00\xe7"
           LONG_NAME2
           "\x00\01\x00\x00");
  tt_int_op(rh.length, OP_EQ, RELAY_PAYLOAD_SIZE);
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(addrs), OP_EQ, 2);
  a = smartlist_get(addrs, 0);
  tt_str_op(a->hostname, OP_EQ, LONG_NAME);
  a = smartlist_get(addrs, 1);
  tt_str_op(a->hostname, OP_EQ, LONG_NAME2);
  CLEAR_ADDRS();

  /* BAD CELLS */

  /* Invalid length on an IPv4 */
  SET_CELL("\x04\x03zzz1234");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  SET_CELL("\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00"
           "\x04\x05zzzzz1234");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  /* Invalid length on an IPv6 */
  SET_CELL("\x06\x03zzz1234");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  SET_CELL("\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00"
           "\x06\x17wwwwwwwwwwwwwwwww1234");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  SET_CELL("\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00"
           "\x06\x10xxxx");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  /* Empty hostname */
  SET_CELL("\x00\x00xxxx");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  /* rh.length out of range */
  CLEAR_CELL();
  rh.length = 499;
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(errcode, OP_EQ, 0);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  /* Item length extends beyond rh.length */
  CLEAR_CELL();
  SET_CELL("\x00\xff"
           LONG_NAME
           "\x00\01\x00\x00");
  rh.length -= 1;
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);
  rh.length -= 5;
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  SET_CELL("\x04\x04" "\x7f\x00\x02\x0a" "\x00\00\x01\x00");
  rh.length -= 1;
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  SET_CELL("\xee\x10"
           "\x20\x02\x90\x01\x00\x00\x00\x00"
           "\x00\x00\x00\x00\x00\xfa\xca\xde"
           "\x00\00\x00\x03");
  rh.length -= 1;
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  /* Truncated item after first character */
  SET_CELL("\x04");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

  SET_CELL("\xee");
  r = resolved_cell_parse(&cell, &rh, addrs, &errcode);
  tt_int_op(r, OP_EQ, -1);
  tt_int_op(smartlist_len(addrs), OP_EQ, 0);

 done:
  CLEAR_ADDRS();
  CLEAR_CELL();
  smartlist_free(addrs);
#undef CLEAR_ADDRS
#undef CLEAR_CELL
}

static void
test_cfmt_is_destroy(void *arg)
{
  cell_t cell;
  packed_cell_t packed;
  circid_t circid = 0;
  channel_t *chan;
  (void)arg;

  chan = tor_malloc_zero(sizeof(channel_t));

  memset(&cell, 0xff, sizeof(cell));
  cell.circ_id = 3003;
  cell.command = CELL_RELAY;

  cell_pack(&packed, &cell, 0);
  chan->wide_circ_ids = 0;
  tt_assert(! packed_cell_is_destroy(chan, &packed, &circid));
  tt_int_op(circid, OP_EQ, 0);

  cell_pack(&packed, &cell, 1);
  chan->wide_circ_ids = 1;
  tt_assert(! packed_cell_is_destroy(chan, &packed, &circid));
  tt_int_op(circid, OP_EQ, 0);

  cell.command = CELL_DESTROY;

  cell_pack(&packed, &cell, 0);
  chan->wide_circ_ids = 0;
  tt_assert(packed_cell_is_destroy(chan, &packed, &circid));
  tt_int_op(circid, OP_EQ, 3003);

  circid = 0;
  cell_pack(&packed, &cell, 1);
  chan->wide_circ_ids = 1;
  tt_assert(packed_cell_is_destroy(chan, &packed, &circid));

 done:
  tor_free(chan);
}

#define TEST(name, flags)                                               \
  { #name, test_cfmt_ ## name, flags, 0, NULL }

struct testcase_t cell_format_tests[] = {
  TEST(relay_header, 0),
  TEST(begin_cells, 0),
  TEST(connected_cells, 0),
  TEST(create_cells, 0),
  TEST(created_cells, 0),
  TEST(extend_cells, 0),
  TEST(extended_cells, 0),
  TEST(resolved_cells, 0),
  TEST(is_destroy, 0),
  END_OF_TESTCASES
};

