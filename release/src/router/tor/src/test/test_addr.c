/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define ADDRESSMAP_PRIVATE
#include "orconfig.h"
#include "or.h"
#include "test.h"
#include "addressmap.h"

static void
test_addr_basic(void *arg)
{
  uint32_t u32;
  uint16_t u16;
  char *cp;

  /* Test addr_port_lookup */
  (void)arg;
  cp = NULL; u32 = 3; u16 = 3;
  tt_assert(!addr_port_lookup(LOG_WARN, "1.2.3.4", &cp, &u32, &u16));
  tt_str_op(cp,OP_EQ, "1.2.3.4");
  tt_int_op(u32,OP_EQ, 0x01020304u);
  tt_int_op(u16,OP_EQ, 0);
  tor_free(cp);
  tt_assert(!addr_port_lookup(LOG_WARN, "4.3.2.1:99", &cp, &u32, &u16));
  tt_str_op(cp,OP_EQ, "4.3.2.1");
  tt_int_op(u32,OP_EQ, 0x04030201u);
  tt_int_op(u16,OP_EQ, 99);
  tor_free(cp);
  tt_assert(!addr_port_lookup(LOG_WARN, "nonexistent.address:4040",
                               &cp, NULL, &u16));
  tt_str_op(cp,OP_EQ, "nonexistent.address");
  tt_int_op(u16,OP_EQ, 4040);
  tor_free(cp);
  tt_assert(!addr_port_lookup(LOG_WARN, "localhost:9999", &cp, &u32, &u16));
  tt_str_op(cp,OP_EQ, "localhost");
  tt_int_op(u32,OP_EQ, 0x7f000001u);
  tt_int_op(u16,OP_EQ, 9999);
  tor_free(cp);
  u32 = 3;
  tt_assert(!addr_port_lookup(LOG_WARN, "localhost", NULL, &u32, &u16));
  tt_ptr_op(cp,OP_EQ, NULL);
  tt_int_op(u32,OP_EQ, 0x7f000001u);
  tt_int_op(u16,OP_EQ, 0);
  tor_free(cp);

  tt_assert(addr_port_lookup(LOG_WARN, "localhost:3", &cp, &u32, NULL));
  tor_free(cp);

  tt_int_op(0,OP_EQ, addr_mask_get_bits(0x0u));
  tt_int_op(32,OP_EQ, addr_mask_get_bits(0xFFFFFFFFu));
  tt_int_op(16,OP_EQ, addr_mask_get_bits(0xFFFF0000u));
  tt_int_op(31,OP_EQ, addr_mask_get_bits(0xFFFFFFFEu));
  tt_int_op(1,OP_EQ, addr_mask_get_bits(0x80000000u));

  /* Test inet_ntop */
  {
    char tmpbuf[TOR_ADDR_BUF_LEN];
    const char *ip = "176.192.208.224";
    struct in_addr in;

    /* good round trip */
    tt_int_op(tor_inet_pton(AF_INET, ip, &in), OP_EQ, 1);
    tt_ptr_op(tor_inet_ntop(AF_INET, &in, tmpbuf, sizeof(tmpbuf)),
              OP_EQ, &tmpbuf);
    tt_str_op(tmpbuf,OP_EQ, ip);

    /* just enough buffer length */
    tt_str_op(tor_inet_ntop(AF_INET, &in, tmpbuf, strlen(ip) + 1), OP_EQ, ip);

    /* too short buffer */
    tt_ptr_op(tor_inet_ntop(AF_INET, &in, tmpbuf, strlen(ip)),OP_EQ, NULL);
  }

 done:
  tor_free(cp);
}

#define test_op_ip6_(a,op,b,e1,e2)                               \
  STMT_BEGIN                                                     \
  tt_assert_test_fmt_type(a,b,e1" "#op" "e2,struct in6_addr*,    \
    (memcmp(val1_->s6_addr, val2_->s6_addr, 16) op 0),           \
    char *, "%s",                                                \
    { int i; char *cp;                                           \
      cp = print_ = tor_malloc(64);                              \
      for (i=0;i<16;++i) {                                       \
        tor_snprintf(cp, 3,"%02x", (unsigned)value_->s6_addr[i]);\
        cp += 2;                                                 \
        if (i != 15) *cp++ = ':';                                \
      }                                                          \
    },                                                           \
    { tor_free(print_); },                                       \
    TT_EXIT_TEST_FUNCTION                                        \
  );                                                             \
  STMT_END

/** Helper: Assert that two strings both decode as IPv6 addresses with
 * tor_inet_pton(), and both decode to the same address. */
#define test_pton6_same(a,b) STMT_BEGIN                 \
     tt_int_op(tor_inet_pton(AF_INET6, a, &a1), OP_EQ, 1); \
     tt_int_op(tor_inet_pton(AF_INET6, b, &a2), OP_EQ, 1); \
     test_op_ip6_(&a1,OP_EQ,&a2,#a,#b);                    \
  STMT_END

/** Helper: Assert that <b>a</b> is recognized as a bad IPv6 address by
 * tor_inet_pton(). */
#define test_pton6_bad(a)                       \
  tt_int_op(0, OP_EQ, tor_inet_pton(AF_INET6, a, &a1))

/** Helper: assert that <b>a</b>, when parsed by tor_inet_pton() and displayed
 * with tor_inet_ntop(), yields <b>b</b>. Also assert that <b>b</b> parses to
 * the same value as <b>a</b>. */
#define test_ntop6_reduces(a,b) STMT_BEGIN                          \
  tt_int_op(tor_inet_pton(AF_INET6, a, &a1), OP_EQ, 1);                \
  tt_str_op(tor_inet_ntop(AF_INET6, &a1, buf, sizeof(buf)), OP_EQ, b); \
  tt_int_op(tor_inet_pton(AF_INET6, b, &a2), OP_EQ, 1);     \
  test_op_ip6_(&a1, OP_EQ, &a2, a, b);                      \
  STMT_END

/** Helper: assert that <b>a</b> parses by tor_inet_pton() into a address that
 * passes tor_addr_is_internal() with <b>for_listening</b>. */
#define test_internal_ip(a,for_listening) STMT_BEGIN           \
    tt_int_op(tor_inet_pton(AF_INET6, a, &t1.addr.in6_addr), OP_EQ, 1); \
    t1.family = AF_INET6;                                      \
    if (!tor_addr_is_internal(&t1, for_listening))             \
      TT_DIE(("%s was not internal", a));                      \
  STMT_END

/** Helper: assert that <b>a</b> parses by tor_inet_pton() into a address that
 * does not pass tor_addr_is_internal() with <b>for_listening</b>. */
#define test_external_ip(a,for_listening) STMT_BEGIN           \
    tt_int_op(tor_inet_pton(AF_INET6, a, &t1.addr.in6_addr), OP_EQ, 1); \
    t1.family = AF_INET6;                                      \
    if (tor_addr_is_internal(&t1, for_listening))              \
      TT_DIE(("%s was not internal", a));                      \
  STMT_END

/** Helper: Assert that <b>a</b> and <b>b</b>, when parsed by
 * tor_inet_pton(), give addresses that compare in the order defined by
 * <b>op</b> with tor_addr_compare(). */
#define test_addr_compare(a, op, b) STMT_BEGIN                    \
    tt_int_op(tor_inet_pton(AF_INET6, a, &t1.addr.in6_addr), OP_EQ, 1); \
    tt_int_op(tor_inet_pton(AF_INET6, b, &t2.addr.in6_addr), OP_EQ, 1); \
    t1.family = t2.family = AF_INET6;                             \
    r = tor_addr_compare(&t1,&t2,CMP_SEMANTIC);                   \
    if (!(r op 0))                                                \
      TT_DIE(("Failed: tor_addr_compare(%s,%s) %s 0", a, b, #op));\
  STMT_END

/** Helper: Assert that <b>a</b> and <b>b</b>, when parsed by
 * tor_inet_pton(), give addresses that compare in the order defined by
 * <b>op</b> with tor_addr_compare_masked() with <b>m</b> masked. */
#define test_addr_compare_masked(a, op, b, m) STMT_BEGIN          \
    tt_int_op(tor_inet_pton(AF_INET6, a, &t1.addr.in6_addr), OP_EQ, 1);    \
    tt_int_op(tor_inet_pton(AF_INET6, b, &t2.addr.in6_addr), OP_EQ, 1);    \
    t1.family = t2.family = AF_INET6;                             \
    r = tor_addr_compare_masked(&t1,&t2,m,CMP_SEMANTIC);          \
    if (!(r op 0))                                                \
      TT_DIE(("Failed: tor_addr_compare_masked(%s,%s,%d) %s 0", \
              a, b, m, #op));                                   \
  STMT_END

/** Helper: assert that <b>xx</b> is parseable as a masked IPv6 address with
 * ports by tor_parse_mask_addr_ports(), with family <b>f</b>, IP address
 * as 4 32-bit words <b>ip1...ip4</b>, mask bits as <b>mm</b>, and port range
 * as <b>pt1..pt2</b>. */
#define test_addr_mask_ports_parse(xx, f, ip1, ip2, ip3, ip4, mm, pt1, pt2) \
  STMT_BEGIN                                                                \
    tt_int_op(tor_addr_parse_mask_ports(xx, 0, &t1, &mask, &port1, &port2),   \
              OP_EQ, f);                                                   \
    p1=tor_inet_ntop(AF_INET6, &t1.addr.in6_addr, bug, sizeof(bug));        \
    tt_int_op(htonl(ip1), OP_EQ, tor_addr_to_in6_addr32(&t1)[0]);            \
    tt_int_op(htonl(ip2), OP_EQ, tor_addr_to_in6_addr32(&t1)[1]);            \
    tt_int_op(htonl(ip3), OP_EQ, tor_addr_to_in6_addr32(&t1)[2]);            \
    tt_int_op(htonl(ip4), OP_EQ, tor_addr_to_in6_addr32(&t1)[3]);            \
    tt_int_op(mask, OP_EQ, mm);                     \
    tt_uint_op(port1, OP_EQ, pt1);                  \
    tt_uint_op(port2, OP_EQ, pt2);                  \
  STMT_END

/** Run unit tests for IPv6 encoding/decoding/manipulation functions. */
static void
test_addr_ip6_helpers(void *arg)
{
  char buf[TOR_ADDR_BUF_LEN], bug[TOR_ADDR_BUF_LEN];
  char rbuf[REVERSE_LOOKUP_NAME_BUF_LEN];
  struct in6_addr a1, a2;
  tor_addr_t t1, t2;
  int r, i;
  uint16_t port1, port2;
  maskbits_t mask;
  const char *p1;
  struct sockaddr_storage sa_storage;
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;

  /* Test tor_inet_ntop and tor_inet_pton: IPv6 */
  (void)arg;
  {
    const char *ip = "2001::1234";
    const char *ip_ffff = "::ffff:192.168.1.2";

    /* good round trip */
    tt_int_op(tor_inet_pton(AF_INET6, ip, &a1),OP_EQ, 1);
    tt_ptr_op(tor_inet_ntop(AF_INET6, &a1, buf, sizeof(buf)),OP_EQ, &buf);
    tt_str_op(buf,OP_EQ, ip);

    /* good round trip - ::ffff:0:0 style */
    tt_int_op(tor_inet_pton(AF_INET6, ip_ffff, &a2),OP_EQ, 1);
    tt_ptr_op(tor_inet_ntop(AF_INET6, &a2, buf, sizeof(buf)),OP_EQ, &buf);
    tt_str_op(buf,OP_EQ, ip_ffff);

    /* just long enough buffer (remember \0) */
    tt_str_op(tor_inet_ntop(AF_INET6, &a1, buf, strlen(ip)+1),OP_EQ, ip);
    tt_str_op(tor_inet_ntop(AF_INET6, &a2, buf, strlen(ip_ffff)+1),OP_EQ,
               ip_ffff);

    /* too short buffer (remember \0) */
    tt_ptr_op(tor_inet_ntop(AF_INET6, &a1, buf, strlen(ip)),OP_EQ, NULL);
    tt_ptr_op(tor_inet_ntop(AF_INET6, &a2, buf, strlen(ip_ffff)),OP_EQ, NULL);
  }

  /* ==== Converting to and from sockaddr_t. */
  sin = (struct sockaddr_in *)&sa_storage;
  sin->sin_family = AF_INET;
  sin->sin_port = htons(9090);
  sin->sin_addr.s_addr = htonl(0x7f7f0102); /*127.127.1.2*/
  tor_addr_from_sockaddr(&t1, (struct sockaddr *)sin, &port1);
  tt_int_op(tor_addr_family(&t1),OP_EQ, AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ, 0x7f7f0102);
  tt_int_op(port1, OP_EQ, 9090);

  memset(&sa_storage, 0, sizeof(sa_storage));
  tt_int_op(sizeof(struct sockaddr_in),OP_EQ,
          tor_addr_to_sockaddr(&t1, 1234, (struct sockaddr *)&sa_storage,
                               sizeof(sa_storage)));
  tt_int_op(1234,OP_EQ, ntohs(sin->sin_port));
  tt_int_op(0x7f7f0102,OP_EQ, ntohl(sin->sin_addr.s_addr));

  memset(&sa_storage, 0, sizeof(sa_storage));
  sin6 = (struct sockaddr_in6 *)&sa_storage;
  sin6->sin6_family = AF_INET6;
  sin6->sin6_port = htons(7070);
  sin6->sin6_addr.s6_addr[0] = 128;
  tor_addr_from_sockaddr(&t1, (struct sockaddr *)sin6, &port1);
  tt_int_op(tor_addr_family(&t1),OP_EQ, AF_INET6);
  tt_int_op(port1, OP_EQ, 7070);
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 0);
  tt_str_op(p1,OP_EQ, "8000::");

  memset(&sa_storage, 0, sizeof(sa_storage));
  tt_int_op(sizeof(struct sockaddr_in6),OP_EQ,
          tor_addr_to_sockaddr(&t1, 9999, (struct sockaddr *)&sa_storage,
                               sizeof(sa_storage)));
  tt_int_op(AF_INET6,OP_EQ, sin6->sin6_family);
  tt_int_op(9999,OP_EQ, ntohs(sin6->sin6_port));
  tt_int_op(0x80000000,OP_EQ, ntohl(S6_ADDR32(sin6->sin6_addr)[0]));

  /* ==== tor_addr_lookup: static cases.  (Can't test dns without knowing we
   * have a good resolver. */
  tt_int_op(0,OP_EQ, tor_addr_lookup("127.128.129.130", AF_UNSPEC, &t1));
  tt_int_op(AF_INET,OP_EQ, tor_addr_family(&t1));
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ, 0x7f808182);

  tt_int_op(0,OP_EQ, tor_addr_lookup("9000::5", AF_UNSPEC, &t1));
  tt_int_op(AF_INET6,OP_EQ, tor_addr_family(&t1));
  tt_int_op(0x90,OP_EQ, tor_addr_to_in6_addr8(&t1)[0]);
  tt_assert(tor_mem_is_zero((char*)tor_addr_to_in6_addr8(&t1)+1, 14));
  tt_int_op(0x05,OP_EQ, tor_addr_to_in6_addr8(&t1)[15]);

  /* === Test pton: valid af_inet6 */
  /* Simple, valid parsing. */
  r = tor_inet_pton(AF_INET6,
                    "0102:0304:0506:0708:090A:0B0C:0D0E:0F10", &a1);
  tt_int_op(r, OP_EQ, 1);
  for (i=0;i<16;++i) { tt_int_op(i+1,OP_EQ, (int)a1.s6_addr[i]); }
  /* ipv4 ending. */
  test_pton6_same("0102:0304:0506:0708:090A:0B0C:0D0E:0F10",
                  "0102:0304:0506:0708:090A:0B0C:13.14.15.16");
  /* shortened words. */
  test_pton6_same("0001:0099:BEEF:0000:0123:FFFF:0001:0001",
                  "1:99:BEEF:0:0123:FFFF:1:1");
  /* zeros at the beginning */
  test_pton6_same("0000:0000:0000:0000:0009:C0A8:0001:0001",
                  "::9:c0a8:1:1");
  test_pton6_same("0000:0000:0000:0000:0009:C0A8:0001:0001",
                  "::9:c0a8:0.1.0.1");
  /* zeros in the middle. */
  test_pton6_same("fe80:0000:0000:0000:0202:1111:0001:0001",
                  "fe80::202:1111:1:1");
  /* zeros at the end. */
  test_pton6_same("1000:0001:0000:0007:0000:0000:0000:0000",
                  "1000:1:0:7::");

  /* === Test ntop: af_inet6 */
  test_ntop6_reduces("0:0:0:0:0:0:0:0", "::");

  test_ntop6_reduces("0001:0099:BEEF:0006:0123:FFFF:0001:0001",
                     "1:99:beef:6:123:ffff:1:1");

  //test_ntop6_reduces("0:0:0:0:0:0:c0a8:0101", "::192.168.1.1");
  test_ntop6_reduces("0:0:0:0:0:ffff:c0a8:0101", "::ffff:192.168.1.1");
  test_ntop6_reduces("002:0:0000:0:3::4", "2::3:0:0:4");
  test_ntop6_reduces("0:0::1:0:3", "::1:0:3");
  test_ntop6_reduces("008:0::0", "8::");
  test_ntop6_reduces("0:0:0:0:0:ffff::1", "::ffff:0.0.0.1");
  test_ntop6_reduces("abcd:0:0:0:0:0:7f00::", "abcd::7f00:0");
  test_ntop6_reduces("0000:0000:0000:0000:0009:C0A8:0001:0001",
                     "::9:c0a8:1:1");
  test_ntop6_reduces("fe80:0000:0000:0000:0202:1111:0001:0001",
                     "fe80::202:1111:1:1");
  test_ntop6_reduces("1000:0001:0000:0007:0000:0000:0000:0000",
                     "1000:1:0:7::");

  /* Bad af param */
  tt_int_op(tor_inet_pton(AF_UNSPEC, 0, 0),OP_EQ, -1);

  /* === Test pton: invalid in6. */
  test_pton6_bad("foobar.");
  test_pton6_bad("-1::");
  test_pton6_bad("00001::");
  test_pton6_bad("10000::");
  test_pton6_bad("::10000");
  test_pton6_bad("55555::");
  test_pton6_bad("9:-60::");
  test_pton6_bad("9:+60::");
  test_pton6_bad("9|60::");
  test_pton6_bad("0x60::");
  test_pton6_bad("::0x60");
  test_pton6_bad("9:0x60::");
  test_pton6_bad("1:2:33333:4:0002:3::");
  test_pton6_bad("1:2:3333:4:fish:3::");
  test_pton6_bad("1:2:3:4:5:6:7:8:9");
  test_pton6_bad("1:2:3:4:5:6:7");
  test_pton6_bad("1:2:3:4:5:6:1.2.3.4.5");
  test_pton6_bad("1:2:3:4:5:6:1.2.3");
  test_pton6_bad("::1.2.3");
  test_pton6_bad("::1.2.3.4.5");
  test_pton6_bad("::ffff:0xff.0.0.0");
  test_pton6_bad("::ffff:ff.0.0.0");
  test_pton6_bad("::ffff:256.0.0.0");
  test_pton6_bad("::ffff:-1.0.0.0");
  test_pton6_bad("99");
  test_pton6_bad("");
  test_pton6_bad(".");
  test_pton6_bad(":");
  test_pton6_bad("1::2::3:4");
  test_pton6_bad("a:::b:c");
  test_pton6_bad(":::a:b:c");
  test_pton6_bad("a:b:c:::");
  test_pton6_bad("1.2.3.4");
  test_pton6_bad(":1.2.3.4");
  test_pton6_bad(".2.3.4");

  /* test internal checking */
  test_external_ip("fbff:ffff::2:7", 0);
  test_internal_ip("fc01::2:7", 0);
  test_internal_ip("fc01::02:7", 0);
  test_internal_ip("fc01::002:7", 0);
  test_internal_ip("fc01::0002:7", 0);
  test_internal_ip("fdff:ffff::f:f", 0);
  test_external_ip("fe00::3:f", 0);

  test_external_ip("fe7f:ffff::2:7", 0);
  test_internal_ip("fe80::2:7", 0);
  test_internal_ip("febf:ffff::f:f", 0);

  test_internal_ip("fec0::2:7:7", 0);
  test_internal_ip("feff:ffff::e:7:7", 0);
  test_external_ip("ff00::e:7:7", 0);

  test_internal_ip("::", 0);
  test_internal_ip("::1", 0);
  test_internal_ip("::1", 1);
  test_internal_ip("::", 0);
  test_external_ip("::", 1);
  test_external_ip("::2", 0);
  test_external_ip("2001::", 0);
  test_external_ip("ffff::", 0);

  test_external_ip("::ffff:0.0.0.0", 1);
  test_internal_ip("::ffff:0.0.0.0", 0);
  test_internal_ip("::ffff:0.255.255.255", 0);
  test_external_ip("::ffff:1.0.0.0", 0);

  test_external_ip("::ffff:9.255.255.255", 0);
  test_internal_ip("::ffff:10.0.0.0", 0);
  test_internal_ip("::ffff:10.255.255.255", 0);
  test_external_ip("::ffff:11.0.0.0", 0);

  test_external_ip("::ffff:126.255.255.255", 0);
  test_internal_ip("::ffff:127.0.0.0", 0);
  test_internal_ip("::ffff:127.255.255.255", 0);
  test_external_ip("::ffff:128.0.0.0", 0);

  test_external_ip("::ffff:172.15.255.255", 0);
  test_internal_ip("::ffff:172.16.0.0", 0);
  test_internal_ip("::ffff:172.31.255.255", 0);
  test_external_ip("::ffff:172.32.0.0", 0);

  test_external_ip("::ffff:192.167.255.255", 0);
  test_internal_ip("::ffff:192.168.0.0", 0);
  test_internal_ip("::ffff:192.168.255.255", 0);
  test_external_ip("::ffff:192.169.0.0", 0);

  test_external_ip("::ffff:169.253.255.255", 0);
  test_internal_ip("::ffff:169.254.0.0", 0);
  test_internal_ip("::ffff:169.254.255.255", 0);
  test_external_ip("::ffff:169.255.0.0", 0);

  /* tor_addr_compare(tor_addr_t x2) */
  test_addr_compare("ffff::", OP_EQ, "ffff::0");
  test_addr_compare("0::3:2:1", OP_LT, "0::ffff:0.3.2.1");
  test_addr_compare("0::2:2:1", OP_LT, "0::ffff:0.3.2.1");
  test_addr_compare("0::ffff:0.3.2.1", OP_GT, "0::0:0:0");
  test_addr_compare("0::ffff:5.2.2.1", OP_LT,
                    "::ffff:6.0.0.0"); /* XXXX wrong. */
  tor_addr_parse_mask_ports("[::ffff:2.3.4.5]", 0, &t1, NULL, NULL, NULL);
  tor_addr_parse_mask_ports("2.3.4.5", 0, &t2, NULL, NULL, NULL);
  tt_assert(tor_addr_compare(&t1, &t2, CMP_SEMANTIC) == 0);
  tor_addr_parse_mask_ports("[::ffff:2.3.4.4]", 0, &t1, NULL, NULL, NULL);
  tor_addr_parse_mask_ports("2.3.4.5", 0, &t2, NULL, NULL, NULL);
  tt_assert(tor_addr_compare(&t1, &t2, CMP_SEMANTIC) < 0);

  /* test compare_masked */
  test_addr_compare_masked("ffff::", OP_EQ, "ffff::0", 128);
  test_addr_compare_masked("ffff::", OP_EQ, "ffff::0", 64);
  test_addr_compare_masked("0::2:2:1", OP_LT, "0::8000:2:1", 81);
  test_addr_compare_masked("0::2:2:1", OP_EQ, "0::8000:2:1", 80);

  /* Test undecorated tor_addr_to_str */
  tt_int_op(AF_INET6,OP_EQ, tor_addr_parse(&t1, "[123:45:6789::5005:11]"));
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 0);
  tt_str_op(p1,OP_EQ, "123:45:6789::5005:11");
  tt_int_op(AF_INET,OP_EQ, tor_addr_parse(&t1, "18.0.0.1"));
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 0);
  tt_str_op(p1,OP_EQ, "18.0.0.1");

  /* Test decorated tor_addr_to_str */
  tt_int_op(AF_INET6,OP_EQ, tor_addr_parse(&t1, "[123:45:6789::5005:11]"));
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 1);
  tt_str_op(p1,OP_EQ, "[123:45:6789::5005:11]");
  tt_int_op(AF_INET,OP_EQ, tor_addr_parse(&t1, "18.0.0.1"));
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 1);
  tt_str_op(p1,OP_EQ, "18.0.0.1");

  /* Test buffer bounds checking of tor_addr_to_str */
  tt_int_op(AF_INET6,OP_EQ, tor_addr_parse(&t1, "::")); /* 2 + \0 */
  tt_ptr_op(tor_addr_to_str(buf, &t1, 2, 0),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 3, 0),OP_EQ, "::");
  tt_ptr_op(tor_addr_to_str(buf, &t1, 4, 1),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 5, 1),OP_EQ, "[::]");

  tt_int_op(AF_INET6,OP_EQ, tor_addr_parse(&t1, "2000::1337")); /* 10 + \0 */
  tt_ptr_op(tor_addr_to_str(buf, &t1, 10, 0),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 11, 0),OP_EQ, "2000::1337");
  tt_ptr_op(tor_addr_to_str(buf, &t1, 12, 1),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 13, 1),OP_EQ, "[2000::1337]");

  tt_int_op(AF_INET,OP_EQ, tor_addr_parse(&t1, "1.2.3.4")); /* 7 + \0 */
  tt_ptr_op(tor_addr_to_str(buf, &t1, 7, 0),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 8, 0),OP_EQ, "1.2.3.4");

  tt_int_op(AF_INET, OP_EQ,
            tor_addr_parse(&t1, "255.255.255.255")); /* 15 + \0 */
  tt_ptr_op(tor_addr_to_str(buf, &t1, 15, 0),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 16, 0),OP_EQ, "255.255.255.255");
  tt_ptr_op(tor_addr_to_str(buf, &t1, 15, 1),OP_EQ, NULL); /* too short buf */
  tt_str_op(tor_addr_to_str(buf, &t1, 16, 1),OP_EQ, "255.255.255.255");

  t1.family = AF_UNSPEC;
  tt_ptr_op(tor_addr_to_str(buf, &t1, sizeof(buf), 0),OP_EQ, NULL);

  /* Test tor_addr_parse_PTR_name */
  i = tor_addr_parse_PTR_name(&t1, "Foobar.baz", AF_UNSPEC, 0);
  tt_int_op(0,OP_EQ, i);
  i = tor_addr_parse_PTR_name(&t1, "Foobar.baz", AF_UNSPEC, 1);
  tt_int_op(0,OP_EQ, i);
  i = tor_addr_parse_PTR_name(&t1, "9999999999999999999999999999.in-addr.arpa",
                              AF_UNSPEC, 1);
  tt_int_op(-1,OP_EQ, i);
  i = tor_addr_parse_PTR_name(&t1, "1.0.168.192.in-addr.arpa",
                                         AF_UNSPEC, 1);
  tt_int_op(1,OP_EQ, i);
  tt_int_op(tor_addr_family(&t1),OP_EQ, AF_INET);
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 1);
  tt_str_op(p1,OP_EQ, "192.168.0.1");
  i = tor_addr_parse_PTR_name(&t1, "192.168.0.99", AF_UNSPEC, 0);
  tt_int_op(0,OP_EQ, i);
  i = tor_addr_parse_PTR_name(&t1, "192.168.0.99", AF_UNSPEC, 1);
  tt_int_op(1,OP_EQ, i);
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 1);
  tt_str_op(p1,OP_EQ, "192.168.0.99");
  memset(&t1, 0, sizeof(t1));
  i = tor_addr_parse_PTR_name(&t1,
                                         "0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f."
                                         "f.e.e.b.1.e.b.e.e.f.f.e.e.e.d.9."
                                         "ip6.ARPA",
                                         AF_UNSPEC, 0);
  tt_int_op(1,OP_EQ, i);
  p1 = tor_addr_to_str(buf, &t1, sizeof(buf), 1);
  tt_str_op(p1,OP_EQ, "[9dee:effe:ebe1:beef:fedc:ba98:7654:3210]");
  /* Failing cases. */
  i = tor_addr_parse_PTR_name(&t1,
                                         "6.7.8.9.a.b.c.d.e.f."
                                         "f.e.e.b.1.e.b.e.e.f.f.e.e.e.d.9."
                                         "ip6.ARPA",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1,
                                         "6.7.8.9.a.b.c.d.e.f.a.b.c.d.e.f.0."
                                         "f.e.e.b.1.e.b.e.e.f.f.e.e.e.d.9."
                                         "ip6.ARPA",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1,
                                         "6.7.8.9.a.b.c.d.e.f.X.0.0.0.0.9."
                                         "f.e.e.b.1.e.b.e.e.f.f.e.e.e.d.9."
                                         "ip6.ARPA",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1, "32.1.1.in-addr.arpa",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1, ".in-addr.arpa",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1, "1.2.3.4.5.in-addr.arpa",
                                         AF_UNSPEC, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1, "1.2.3.4.5.in-addr.arpa",
                                         AF_INET6, 0);
  tt_int_op(i,OP_EQ, -1);
  i = tor_addr_parse_PTR_name(&t1,
                                         "6.7.8.9.a.b.c.d.e.f.a.b.c.d.e.0."
                                         "f.e.e.b.1.e.b.e.e.f.f.e.e.e.d.9."
                                         "ip6.ARPA",
                                         AF_INET, 0);
  tt_int_op(i,OP_EQ, -1);

  /* === Test tor_addr_to_PTR_name */

  /* Stage IPv4 addr */
  memset(&sa_storage, 0, sizeof(sa_storage));
  sin = (struct sockaddr_in *)&sa_storage;
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl(0x7f010203); /* 127.1.2.3 */
  tor_addr_from_sockaddr(&t1, (struct sockaddr *)sin, NULL);

  /* Check IPv4 PTR - too short buffer */
  tt_int_op(tor_addr_to_PTR_name(rbuf, 1, &t1),OP_EQ, -1);
  tt_int_op(tor_addr_to_PTR_name(rbuf,
                               strlen("3.2.1.127.in-addr.arpa") - 1,
                               &t1),OP_EQ, -1);

  /* Check IPv4 PTR - valid addr */
  tt_int_op(tor_addr_to_PTR_name(rbuf, sizeof(rbuf), &t1),OP_EQ,
          strlen("3.2.1.127.in-addr.arpa"));
  tt_str_op(rbuf,OP_EQ, "3.2.1.127.in-addr.arpa");

  /* Invalid addr family */
  t1.family = AF_UNSPEC;
  tt_int_op(tor_addr_to_PTR_name(rbuf, sizeof(rbuf), &t1),OP_EQ, -1);

  /* Stage IPv6 addr */
  memset(&sa_storage, 0, sizeof(sa_storage));
  sin6 = (struct sockaddr_in6 *)&sa_storage;
  sin6->sin6_family = AF_INET6;
  sin6->sin6_addr.s6_addr[0] = 0x80; /* 8000::abcd */
  sin6->sin6_addr.s6_addr[14] = 0xab;
  sin6->sin6_addr.s6_addr[15] = 0xcd;

  tor_addr_from_sockaddr(&t1, (struct sockaddr *)sin6, NULL);

  {
    const char* addr_PTR = "d.c.b.a.0.0.0.0.0.0.0.0.0.0.0.0."
      "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.ip6.arpa";

    /* Check IPv6 PTR - too short buffer */
    tt_int_op(tor_addr_to_PTR_name(rbuf, 0, &t1),OP_EQ, -1);
    tt_int_op(tor_addr_to_PTR_name(rbuf, strlen(addr_PTR) - 1, &t1),OP_EQ, -1);

    /* Check IPv6 PTR - valid addr */
    tt_int_op(tor_addr_to_PTR_name(rbuf, sizeof(rbuf), &t1),OP_EQ,
            strlen(addr_PTR));
    tt_str_op(rbuf,OP_EQ, addr_PTR);
  }

  /* XXXX turn this into a separate function; it's not all IPv6. */
  /* test tor_addr_parse_mask_ports */
  test_addr_mask_ports_parse("[::f]/17:47-95", AF_INET6,
                             0, 0, 0, 0x0000000f, 17, 47, 95);
  tt_str_op(p1,OP_EQ, "::f");
  //test_addr_parse("[::fefe:4.1.1.7/120]:999-1000");
  //test_addr_parse_check("::fefe:401:107", 120, 999, 1000);
  test_addr_mask_ports_parse("[::ffff:4.1.1.7]/120:443", AF_INET6,
                             0, 0, 0x0000ffff, 0x04010107, 120, 443, 443);
  tt_str_op(p1,OP_EQ, "::ffff:4.1.1.7");
  test_addr_mask_ports_parse("[abcd:2::44a:0]:2-65000", AF_INET6,
                             0xabcd0002, 0, 0, 0x044a0000, 128, 2, 65000);

  tt_str_op(p1,OP_EQ, "abcd:2::44a:0");
  /* Try some long addresses. */
  r=tor_addr_parse_mask_ports("[ffff:1111:1111:1111:1111:1111:1111:1111]",
                              0, &t1, NULL, NULL, NULL);
  tt_assert(r == AF_INET6);
  r=tor_addr_parse_mask_ports("[ffff:1111:1111:1111:1111:1111:1111:11111]",
                              0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[ffff:1111:1111:1111:1111:1111:1111:1111:1]",
                              0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports(
         "[ffff:1111:1111:1111:1111:1111:1111:ffff:"
         "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:"
         "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:"
         "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]",
         0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  /* Try some failing cases. */
  r=tor_addr_parse_mask_ports("[fefef::]/112", 0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[fefe::/112", 0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[fefe::", 0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[fefe::X]", 0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("efef::/112", 0, &t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[f:f:f:f:f:f:f:f::]",0,&t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[::f:f:f:f:f:f:f:f]",0,&t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[f:f:f:f:f:f:f:f:f]",0,&t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[f:f:f:f:f::]/fred",0,&t1,&mask, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("[f:f:f:f:f::]/255.255.0.0",
                              0,&t1, NULL, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  /* This one will get rejected because it isn't a pure prefix. */
  r=tor_addr_parse_mask_ports("1.1.2.3/255.255.64.0",0,&t1, &mask,NULL,NULL);
  tt_int_op(r, OP_EQ, -1);
  /* Test for V4-mapped address with mask < 96.  (arguably not valid) */
  r=tor_addr_parse_mask_ports("[::ffff:1.1.2.2/33]",0,&t1, &mask, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("1.1.2.2/33",0,&t1, &mask, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  /* Try extended wildcard addresses with out TAPMP_EXTENDED_STAR*/
  r=tor_addr_parse_mask_ports("*4",0,&t1, &mask, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  r=tor_addr_parse_mask_ports("*6",0,&t1, &mask, NULL, NULL);
  tt_int_op(r, OP_EQ, -1);
  tt_assert(r == -1);
  /* Try a mask with a wildcard. */
  r=tor_addr_parse_mask_ports("*/16",0,&t1, &mask, NULL, NULL);
  tt_assert(r == -1);
  r=tor_addr_parse_mask_ports("*4/16",TAPMP_EXTENDED_STAR,
                              &t1, &mask, NULL, NULL);
  tt_assert(r == -1);
  r=tor_addr_parse_mask_ports("*6/30",TAPMP_EXTENDED_STAR,
                              &t1, &mask, NULL, NULL);
  tt_assert(r == -1);
  /* Basic mask tests*/
  r=tor_addr_parse_mask_ports("1.1.2.2/31",0,&t1, &mask, NULL, NULL);
  tt_assert(r == AF_INET);
  tt_int_op(mask,OP_EQ,31);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ,0x01010202);
  r=tor_addr_parse_mask_ports("3.4.16.032:1-2",0,&t1, &mask, &port1, &port2);
  tt_assert(r == AF_INET);
  tt_int_op(mask,OP_EQ,32);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ,0x03041020);
  tt_assert(port1 == 1);
  tt_assert(port2 == 2);
  r=tor_addr_parse_mask_ports("1.1.2.3/255.255.128.0",0,&t1, &mask,NULL,NULL);
  tt_assert(r == AF_INET);
  tt_int_op(mask,OP_EQ,17);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ,0x01010203);
  r=tor_addr_parse_mask_ports("[efef::]/112",0,&t1, &mask, &port1, &port2);
  tt_assert(r == AF_INET6);
  tt_assert(port1 == 1);
  tt_assert(port2 == 65535);
  /* Try regular wildcard behavior without TAPMP_EXTENDED_STAR */
  r=tor_addr_parse_mask_ports("*:80-443",0,&t1,&mask,&port1,&port2);
  tt_int_op(r,OP_EQ,AF_INET); /* Old users of this always get inet */
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ,0);
  tt_int_op(mask,OP_EQ,0);
  tt_int_op(port1,OP_EQ,80);
  tt_int_op(port2,OP_EQ,443);
  /* Now try wildcards *with* TAPMP_EXTENDED_STAR */
  r=tor_addr_parse_mask_ports("*:8000-9000",TAPMP_EXTENDED_STAR,
                              &t1,&mask,&port1,&port2);
  tt_int_op(r,OP_EQ,AF_UNSPEC);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_UNSPEC);
  tt_int_op(mask,OP_EQ,0);
  tt_int_op(port1,OP_EQ,8000);
  tt_int_op(port2,OP_EQ,9000);
  r=tor_addr_parse_mask_ports("*4:6667",TAPMP_EXTENDED_STAR,
                              &t1,&mask,&port1,&port2);
  tt_int_op(r,OP_EQ,AF_INET);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET);
  tt_int_op(tor_addr_to_ipv4h(&t1),OP_EQ,0);
  tt_int_op(mask,OP_EQ,0);
  tt_int_op(port1,OP_EQ,6667);
  tt_int_op(port2,OP_EQ,6667);
  r=tor_addr_parse_mask_ports("*6",TAPMP_EXTENDED_STAR,
                              &t1,&mask,&port1,&port2);
  tt_int_op(r,OP_EQ,AF_INET6);
  tt_int_op(tor_addr_family(&t1),OP_EQ,AF_INET6);
  tt_assert(tor_mem_is_zero((const char*)tor_addr_to_in6_addr32(&t1), 16));
  tt_int_op(mask,OP_EQ,0);
  tt_int_op(port1,OP_EQ,1);
  tt_int_op(port2,OP_EQ,65535);

  /* make sure inet address lengths >= max */
  tt_assert(INET_NTOA_BUF_LEN >= sizeof("255.255.255.255"));
  tt_assert(TOR_ADDR_BUF_LEN >=
              sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"));

  tt_assert(sizeof(tor_addr_t) >= sizeof(struct in6_addr));

  /* get interface addresses */
  r = get_interface_address6(LOG_DEBUG, AF_INET, &t1);
  i = get_interface_address6(LOG_DEBUG, AF_INET6, &t2);

  TT_BLATHER(("v4 address: %s (family=%d)", fmt_addr(&t1),
              tor_addr_family(&t1)));
  TT_BLATHER(("v6 address: %s (family=%d)", fmt_addr(&t2),
              tor_addr_family(&t2)));

 done:
  ;
}

/** Test tor_addr_port_parse(). */
static void
test_addr_parse(void *arg)
{
  int r;
  tor_addr_t addr;
  char buf[TOR_ADDR_BUF_LEN];
  uint16_t port = 0;

  /* Correct call. */
  (void)arg;
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.1:1234",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, 0);
  tor_addr_to_str(buf, &addr, sizeof(buf), 0);
  tt_str_op(buf,OP_EQ, "192.0.2.1");
  tt_int_op(port,OP_EQ, 1234);

  r= tor_addr_port_parse(LOG_DEBUG,
                         "[::1]:1234",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, 0);
  tor_addr_to_str(buf, &addr, sizeof(buf), 0);
  tt_str_op(buf,OP_EQ, "::1");
  tt_int_op(port,OP_EQ, 1234);

  /* Domain name. */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "torproject.org:1234",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);

  /* Only IP. */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.2",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);

  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.2",
                         &addr, &port, 200);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(port,OP_EQ,200);

  r= tor_addr_port_parse(LOG_DEBUG,
                         "[::1]",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);

  r= tor_addr_port_parse(LOG_DEBUG,
                         "[::1]",
                         &addr, &port, 400);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(port,OP_EQ,400);

  /* Bad port. */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.2:66666",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.2:66666",
                         &addr, &port, 200);
  tt_int_op(r, OP_EQ, -1);

  /* Only domain name */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "torproject.org",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);
  r= tor_addr_port_parse(LOG_DEBUG,
                         "torproject.org",
                         &addr, &port, 200);
  tt_int_op(r, OP_EQ, -1);

  /* Bad IP address */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2:1234",
                         &addr, &port, -1);
  tt_int_op(r, OP_EQ, -1);

  /* Make sure that the default port has lower priority than the real
     one */
  r= tor_addr_port_parse(LOG_DEBUG,
                         "192.0.2.2:1337",
                         &addr, &port, 200);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(port,OP_EQ,1337);

  r= tor_addr_port_parse(LOG_DEBUG,
                         "[::1]:1369",
                         &addr, &port, 200);
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(port,OP_EQ,1369);

 done:
  ;
}

static void
update_difference(int ipv6, uint8_t *d,
                  const tor_addr_t *a, const tor_addr_t *b)
{
  const int n_bytes = ipv6 ? 16 : 4;
  uint8_t a_tmp[4], b_tmp[4];
  const uint8_t *ba, *bb;
  int i;

  if (ipv6) {
    ba = tor_addr_to_in6_addr8(a);
    bb = tor_addr_to_in6_addr8(b);
  } else {
    set_uint32(a_tmp, tor_addr_to_ipv4n(a));
    set_uint32(b_tmp, tor_addr_to_ipv4n(b));
    ba = a_tmp; bb = b_tmp;
  }

  for (i = 0; i < n_bytes; ++i) {
    d[i] |= ba[i] ^ bb[i];
  }
}

static void
test_virtaddrmap(void *data)
{
  /* Let's start with a bunch of random addresses. */
  int ipv6, bits, iter, b;
  virtual_addr_conf_t cfg[2];
  uint8_t bytes[16];

  (void)data;

  tor_addr_parse(&cfg[0].addr, "64.65.0.0");
  tor_addr_parse(&cfg[1].addr, "3491:c0c0::");

  for (ipv6 = 0; ipv6 <= 1; ++ipv6) {
    for (bits = 0; bits < 18; ++bits) {
      tor_addr_t last_a;
      cfg[ipv6].bits = bits;
      memset(bytes, 0, sizeof(bytes));
      tor_addr_copy(&last_a, &cfg[ipv6].addr);
      /* Generate 128 addresses with each addr/bits combination. */
      for (iter = 0; iter < 128; ++iter) {
        tor_addr_t a;

        get_random_virtual_addr(&cfg[ipv6], &a);
        //printf("%s\n", fmt_addr(&a));
        /* Make sure that the first b bits match the configured network */
        tt_int_op(0, OP_EQ, tor_addr_compare_masked(&a, &cfg[ipv6].addr,
                                                 bits, CMP_EXACT));

        /* And track which bits have been different between pairs of
         * addresses */
        update_difference(ipv6, bytes, &last_a, &a);
      }

      /* Now make sure all but the first 'bits' bits of bytes are true */
      for (b = bits+1; b < (ipv6?128:32); ++b) {
        tt_assert(1 & (bytes[b/8] >> (7-(b&7))));
      }
    }
  }

 done:
  ;
}

static void
test_addr_localname(void *arg)
{
  (void)arg;
  tt_assert(tor_addr_hostname_is_local("localhost"));
  tt_assert(tor_addr_hostname_is_local("LOCALHOST"));
  tt_assert(tor_addr_hostname_is_local("LocalHost"));
  tt_assert(tor_addr_hostname_is_local("local"));
  tt_assert(tor_addr_hostname_is_local("LOCAL"));
  tt_assert(tor_addr_hostname_is_local("here.now.local"));
  tt_assert(tor_addr_hostname_is_local("here.now.LOCAL"));

  tt_assert(!tor_addr_hostname_is_local(" localhost"));
  tt_assert(!tor_addr_hostname_is_local("www.torproject.org"));
 done:
  ;
}

static void
test_addr_dup_ip(void *arg)
{
  char *v = NULL;
  (void)arg;
#define CHECK(ip, s) do {                         \
    v = tor_dup_ip(ip);                           \
    tt_str_op(v,OP_EQ,(s));                          \
    tor_free(v);                                  \
  } while (0)

  CHECK(0xffffffff, "255.255.255.255");
  CHECK(0x00000000, "0.0.0.0");
  CHECK(0x7f000001, "127.0.0.1");
  CHECK(0x01020304, "1.2.3.4");

#undef CHECK
 done:
  tor_free(v);
}

static void
test_addr_sockaddr_to_str(void *arg)
{
  char *v = NULL;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct sockaddr_storage ss;
#ifdef HAVE_SYS_UN_H
  struct sockaddr_un s_un;
#endif
#define CHECK(sa, s) do {                                       \
    v = tor_sockaddr_to_str((const struct sockaddr*) &(sa));    \
    tt_str_op(v,OP_EQ,(s));                                        \
    tor_free(v);                                                \
  } while (0)
  (void)arg;

  memset(&ss,0,sizeof(ss));
  ss.ss_family = AF_UNSPEC;
  CHECK(ss, "unspec");

  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(0x7f808001);
  sin.sin_port = htons(1234);
  CHECK(sin, "127.128.128.1:1234");

#ifdef HAVE_SYS_UN_H
  memset(&s_un,0,sizeof(s_un));
  s_un.sun_family = AF_UNIX;
  strlcpy(s_un.sun_path, "/here/is/a/path", sizeof(s_un.sun_path));
  CHECK(s_un, "unix:/here/is/a/path");
#endif

  memset(&sin6,0,sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  memcpy(sin6.sin6_addr.s6_addr, "\x20\x00\x00\x00\x00\x00\x00\x00"
                                 "\x00\x1a\x2b\x3c\x4d\x5e\x00\x01", 16);
  sin6.sin6_port = htons(1234);
  CHECK(sin6, "[2000::1a:2b3c:4d5e:1]:1234");

 done:
  tor_free(v);
}

static void
test_addr_is_loopback(void *data)
{
  static const struct loopback_item {
    const char *name;
    int is_loopback;
  } loopback_items[] = {
    { "::1", 1 },
    { "127.0.0.1", 1 },
    { "127.99.100.101", 1 },
    { "128.99.100.101", 0 },
    { "8.8.8.8", 0 },
    { "0.0.0.0", 0 },
    { "::2", 0 },
    { "::", 0 },
    { "::1.0.0.0", 0 },
    { NULL, 0 }
  };

  int i;
  tor_addr_t addr;
  (void)data;

  for (i=0; loopback_items[i].name; ++i) {
    tt_int_op(tor_addr_parse(&addr, loopback_items[i].name), OP_GE, 0);
    tt_int_op(tor_addr_is_loopback(&addr), OP_EQ,
              loopback_items[i].is_loopback);
  }

  tor_addr_make_unspec(&addr);
  tt_int_op(tor_addr_is_loopback(&addr), OP_EQ, 0);

 done:
  ;
}

static void
test_addr_make_null(void *data)
{
  tor_addr_t *addr = tor_malloc(sizeof(*addr));
  tor_addr_t *zeros = tor_malloc_zero(sizeof(*addr));
  char buf[TOR_ADDR_BUF_LEN];
  (void) data;
  /* Ensure that before tor_addr_make_null, addr != 0's */
  memset(addr, 1, sizeof(*addr));
  tt_int_op(memcmp(addr, zeros, sizeof(*addr)), OP_NE, 0);
  /* Test with AF == AF_INET */
  zeros->family = AF_INET;
  tor_addr_make_null(addr, AF_INET);
  tt_int_op(memcmp(addr, zeros, sizeof(*addr)), OP_EQ, 0);
  tt_str_op(tor_addr_to_str(buf, addr, sizeof(buf), 0), OP_EQ, "0.0.0.0");
  /* Test with AF == AF_INET6 */
  memset(addr, 1, sizeof(*addr));
  zeros->family = AF_INET6;
  tor_addr_make_null(addr, AF_INET6);
  tt_int_op(memcmp(addr, zeros, sizeof(*addr)), OP_EQ, 0);
  tt_str_op(tor_addr_to_str(buf, addr, sizeof(buf), 0), OP_EQ, "::");
 done:
  tor_free(addr);
  tor_free(zeros);
}

#define ADDR_LEGACY(name)                                               \
  { #name, test_addr_ ## name , 0, NULL, NULL }

struct testcase_t addr_tests[] = {
  ADDR_LEGACY(basic),
  ADDR_LEGACY(ip6_helpers),
  ADDR_LEGACY(parse),
  { "virtaddr", test_virtaddrmap, 0, NULL, NULL },
  { "localname", test_addr_localname, 0, NULL, NULL },
  { "dup_ip", test_addr_dup_ip, 0, NULL, NULL },
  { "sockaddr_to_str", test_addr_sockaddr_to_str, 0, NULL, NULL },
  { "is_loopback", test_addr_is_loopback, 0, NULL, NULL },
  { "make_null", test_addr_make_null, 0, NULL, NULL },
  END_OF_TESTCASES
};

