/* 
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "vty.h"
#include "stream.h"
#include "privs.h"
#include "memory.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_packet.h"

#define VT100_RESET "\x1b[0m"
#define VT100_RED "\x1b[31m"
#define VT100_GREEN "\x1b[32m"
#define VT100_YELLOW "\x1b[33m"


#define CAPABILITY 0
#define DYNCAP     1
#define OPT_PARAM  2

/* need these to link in libbgp */
struct zebra_privs_t *bgpd_privs = NULL;
struct thread_master *master = NULL;

static int failed = 0;
static int tty = 0;

/* test segments to parse and validate, and use for other tests */
static struct test_segment {
  const char *name;
  const char *desc;
  const u_char data[1024];
  int len;
#define SHOULD_PARSE	0
#define SHOULD_ERR	-1
  int parses; /* whether it should parse or not */
  as_t peek_for; /* what peek_for_as4_capability should say */
  
  /* AFI/SAFI validation */
  int validate_afi;
  afi_t afi;
  safi_t safi;
#define VALID_AFI 1
#define INVALID_AFI 0
  int afi_valid;
} test_segments [] = 
{
  /* 0 */
  { "caphdr", 
    "capability header, and no more",
    { CAPABILITY_CODE_REFRESH, 0x0 },
    2, SHOULD_PARSE,
  },
  /* 1 */
  { "nodata",
    "header, no data but length says there is",
    { 0x1, 0xa },
    2, SHOULD_ERR,
  },
  /* 2 */
  { "padded",
    "valid, with padding",
    { CAPABILITY_CODE_REFRESH, 0x2, 0x0, 0x0 },
    4, SHOULD_PARSE,
  },
  /* 3 */
  { "minsize",
    "violates minsize requirement",
    { CAPABILITY_CODE_ORF, 0x2, 0x0, 0x0 },
    4, SHOULD_ERR,
  },
  { NULL, NULL, {0}, 0, 0},
};

static struct test_segment mp_segments[] =
{
  { "MP4",
    "MP IP/Uni",
    { 0x1, 0x4, 0x0, 0x1, 0x0, 0x1 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP, SAFI_UNICAST, VALID_AFI,
  },
  { "MPv6",
    "MP IPv6/Uni",
    { 0x1, 0x4, 0x0, 0x2, 0x0, 0x1 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP6, SAFI_UNICAST, VALID_AFI,
  },
  /* 5 */
  { "MP2",
    "MP IP/Multicast",
    { CAPABILITY_CODE_MP, 0x4, 0x0, 0x1, 0x0, 0x2 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP, SAFI_MULTICAST, VALID_AFI,
  },
  /* 6 */
  { "MP3",
    "MP IP6/MPLS-labeled VPN",
    { CAPABILITY_CODE_MP, 0x4, 0x0, 0x2, 0x0, 0x80 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP6, SAFI_MPLS_LABELED_VPN, VALID_AFI,
  },
  /* 7 */
  { "MP5",
    "MP IP6/MPLS-VPN",
    { CAPABILITY_CODE_MP, 0x4, 0x0, 0x2, 0x0, 0x4 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP6, SAFI_MPLS_VPN, VALID_AFI,
  },
  /* 8 */
  { "MP6",
    "MP IP4/MPLS-laveled VPN",
    { CAPABILITY_CODE_MP, 0x4, 0x0, 0x1, 0x0, 0x80 },
    6, SHOULD_PARSE, 0,
    1, AFI_IP, SAFI_MPLS_LABELED_VPN, VALID_AFI,
  },  
  /* 10 */
  { "MP8",
    "MP unknown AFI/SAFI",
    { CAPABILITY_CODE_MP, 0x4, 0x0, 0xa, 0x0, 0x81 },
    6, SHOULD_PARSE, 0,
    1, 0xa, 0x81, INVALID_AFI, /* parses, but unknown */
  },
  /* 11 */
  { "MP-short",
    "MP IP4/Unicast, length too short (< minimum)",
    { CAPABILITY_CODE_MP, 0x2, 0x0, 0x1, 0x0, 0x1 },
    6, SHOULD_ERR,
  },
  /* 12 */
  { "MP-overflow",
    "MP IP4/Unicast, length too long",
    { CAPABILITY_CODE_MP, 0x6, 0x0, 0x1, 0x0, 0x1 },
    6, SHOULD_ERR, 0,
    1, AFI_IP, SAFI_UNICAST, VALID_AFI,
  },
  { NULL, NULL, {0}, 0, 0}
};

static struct test_segment misc_segments[] =
{
  /* 13 */
  { "ORF",
    "ORF, simple, single entry, single tuple",
    { /* hdr */		CAPABILITY_CODE_ORF, 0x7, 
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x1, 
      /* tuples */	0x40, 0x3
    },
    9, SHOULD_PARSE,
  },
  /* 14 */
  { "ORF-many",
    "ORF, multi entry/tuple",
    { /* hdr */		CAPABILITY_CODE_ORF, 0x21,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, ORF_MODE_BOTH,
                        0x80, ORF_MODE_RECEIVE,
                        0x80, ORF_MODE_SEND,
      /* mpc */		0x0, 0x2, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, ORF_MODE_BOTH,
                        0x80, ORF_MODE_RECEIVE,
                        0x80, ORF_MODE_SEND,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3, 
      /* tuples */	0x40, ORF_MODE_RECEIVE,
                        0x80, ORF_MODE_SEND,
                        0x80, ORF_MODE_BOTH,
    },
    35, SHOULD_PARSE,
  },
  /* 15 */
  { "ORFlo",
    "ORF, multi entry/tuple, hdr length too short",
    { /* hdr */		CAPABILITY_CODE_ORF, 0x15,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
    },
    35, SHOULD_ERR, /* It should error on invalid Route-Refresh.. */
  },
  /* 16 */
  { "ORFlu",
    "ORF, multi entry/tuple, length too long",
    { /* hdr */		0x3, 0x22,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
    },
    35, SHOULD_ERR
  },
  /* 17 */
  { "ORFnu",
    "ORF, multi entry/tuple, entry number too long",
    { /* hdr */		0x3, 0x21,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x1, 
      /* num */		0x4, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
    },
    35, SHOULD_PARSE, /* parses, but last few tuples should be gibberish */
  },
  /* 18 */
  { "ORFno",
    "ORF, multi entry/tuple, entry number too short",
    { /* hdr */		0x3, 0x21,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x1, 
      /* num */		0x1, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3,
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
    },
    35, SHOULD_PARSE, /* Parses, but should get gibberish afi/safis */
  },
  /* 17 */
  { "ORFpad",
    "ORF, multi entry/tuple, padded to align",
    { /* hdr */		0x3, 0x22,
      /* mpc */		0x0, 0x1, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x1, 
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
      /* mpc */		0x0, 0x2, 0x0, 0x2,
      /* num */		0x3, 
      /* tuples */	0x40, 0x3,
                        0x80, 0x1,
                        0x80, 0x2,
                        0x00,
    },
    36, SHOULD_PARSE,
  },
  /* 19 */
  { "AS4",
    "AS4 capability",
    { 0x41, 0x4, 0xab, 0xcd, 0xef, 0x12 }, /* AS: 2882400018 */
    6, SHOULD_PARSE, 2882400018,
  },
  /* 20 */
  { "GR",
    "GR capability",
    { /* hdr */		CAPABILITY_CODE_RESTART, 0xe,
      /* R-bit, time */	0xf1, 0x12,
      /* afi */		0x0, 0x1,
      /* safi */	0x1,
      /* flags */	0xf,
      /* afi */		0x0, 0x2,
      /* safi */	0x1,
      /* flags */	0x0,
      /* afi */		0x0, 0x2,
      /* safi */	0x2,
      /* flags */	0x1,
    },
    16, SHOULD_PARSE,
  },
  /* 21 */
  { "GR-short",
    "GR capability, but header length too short",
    { /* hdr */		0x40, 0xa,
      /* R-bit, time */	0xf1, 0x12,
      /* afi */		0x0, 0x1,
      /* safi */	0x1,
      /* flags */	0xf,
      /* afi */		0x0, 0x2,
      /* safi */	0x1,
      /* flags */	0x0,
      /* afi */		0x0, 0x2,
      /* safi */	0x2,
      /* flags */	0x1,
    },
    16, SHOULD_PARSE,
  },
  /* 22 */
  { "GR-long",
    "GR capability, but header length too long",
    { /* hdr */		0x40, 0xf,
      /* R-bit, time */	0xf1, 0x12,
      /* afi */		0x0, 0x1,
      /* safi */	0x1,
      /* flags */	0xf,
      /* afi */		0x0, 0x2,
      /* safi */	0x1,
      /* flags */	0x0,
      /* afi */		0x0, 0x2,
      /* safi */	0x2,
    },
    16, SHOULD_ERR,
  },
  { "GR-trunc",
    "GR capability, but truncated",
    { /* hdr */		0x40, 0xf,
      /* R-bit, time */	0xf1, 0x12,
      /* afi */		0x0, 0x1,
      /* safi */	0x1,
      /* flags */	0xf,
      /* afi */		0x0, 0x2,
      /* safi */	0x1,
      /* flags */	0x0,
      /* afi */		0x0, 0x2,
      /* safi */	0x2,
      /* flags */	0x1,
    },
    15, SHOULD_ERR,
  },
  { "GR-empty",
    "GR capability, but empty.",
    { /* hdr */		0x40, 0x0,
    },
    2, SHOULD_ERR,
  },
  { "MP-empty",
    "MP capability, but empty.",
    { /* hdr */		0x1, 0x0,
    },
    2, SHOULD_ERR,
  },
  { "ORF-empty",
    "ORF capability, but empty.",
    { /* hdr */		0x3, 0x0,
    },
    2, SHOULD_ERR,
  },
  { "AS4-empty",
    "AS4 capability, but empty.",
    { /* hdr */		0x41, 0x0,
    },
    2, SHOULD_ERR,
  },
  { "dyn-empty",
    "Dynamic capability, but empty.",
    { /* hdr */		0x42, 0x0,
    },
    2, SHOULD_PARSE,
  },
  { "dyn-old",
    "Dynamic capability (deprecated version)",
    { CAPABILITY_CODE_DYNAMIC, 0x0 },
    2, SHOULD_PARSE,
  },
  { NULL, NULL, {0}, 0, 0}
};

/* DYNAMIC message */
struct test_segment dynamic_cap_msgs[] = 
{
  { "DynCap",
    "Dynamic Capability Message, IP/Multicast",
    { 0x0, 0x1, 0x4, 0x0, 0x1, 0x0, 0x2 },
      7, SHOULD_PARSE, /* horrible alignment, just as with ORF */
  },
  { "DynCapLong",
    "Dynamic Capability Message, IP/Multicast, truncated",
    { 0x0, 0x1, 0x4, 0x0, 0x1, 0x0, 0x2 },
      5, SHOULD_ERR,
  },
  { "DynCapPadded",
    "Dynamic Capability Message, IP/Multicast, padded",
    { 0x0, 0x1, 0x4, 0x0, 0x1, 0x0, 0x2, 0x0 },
      8, SHOULD_ERR, /* No way to tell padding from data.. */
  },
  { "DynCapMPCpadded",
    "Dynamic Capability Message, IP/Multicast, cap data padded",
    { 0x0, 0x1, 0x5, 0x0, 0x1, 0x0, 0x2, 0x0 },
      8, SHOULD_PARSE, /* You can though add padding to the capability data */
  },
  { "DynCapMPCoverflow",
    "Dynamic Capability Message, IP/Multicast, cap data != length",
    { 0x0, 0x1, 0x3, 0x0, 0x1, 0x0, 0x2, 0x0 },
      8, SHOULD_ERR,
  },
  { NULL, NULL, {0}, 0, 0}
};

/* Entire Optional-Parameters block */
struct test_segment opt_params[] =
{
  { "Cap-singlets",
    "One capability per Optional-Param",
    { 0x02, 0x06, 0x01, 0x04, 0x00, 0x01, 0x00, 0x01, /* MP IPv4/Uni */
      0x02, 0x06, 0x01, 0x04, 0x00, 0x02, 0x00, 0x01, /* MP IPv6/Uni */
      0x02, 0x02, 0x80, 0x00, /* RR (old) */
      0x02, 0x02, 0x02, 0x00, /* RR */  
    },
    24, SHOULD_PARSE,
  },
  { "Cap-series",
    "Series of capability, one Optional-Param",
    { 0x02, 0x10,
      0x01, 0x04, 0x00, 0x01, 0x00, 0x01, /* MP IPv4/Uni */
      0x01, 0x04, 0x00, 0x02, 0x00, 0x01, /* MP IPv6/Uni */
      0x80, 0x00, /* RR (old) */
      0x02, 0x00, /* RR */  
    },
    18, SHOULD_PARSE,
  },
  { "AS4more",
    "AS4 capability after other caps (singlets)",
    { 0x02, 0x06, 0x01, 0x04, 0x00, 0x01, 0x00, 0x01, /* MP IPv4/Uni */
      0x02, 0x06, 0x01, 0x04, 0x00, 0x02, 0x00, 0x01, /* MP IPv6/Uni */
      0x02, 0x02, 0x80, 0x00, /* RR (old) */
      0x02, 0x02, 0x02, 0x00, /* RR */
      0x02, 0x06, 0x41, 0x04, 0x00, 0x03, 0x00, 0x06  /* AS4: 1996614 */
    },
    32, SHOULD_PARSE, 196614,
  },
  { "AS4series",
    "AS4 capability, in series of capabilities",
    { 0x02, 0x16,
      0x01, 0x04, 0x00, 0x01, 0x00, 0x01, /* MP IPv4/Uni */
      0x01, 0x04, 0x00, 0x02, 0x00, 0x01, /* MP IPv6/Uni */
      0x80, 0x00, /* RR (old) */
      0x02, 0x00, /* RR */  
      0x41, 0x04, 0x00, 0x03, 0x00, 0x06  /* AS4: 1996614 */
    },
    24, SHOULD_PARSE, 196614,
  },
  { "AS4real",
    "AS4 capability, in series of capabilities",
    {
      0x02, 0x06, 0x01, 0x04, 0x00, 0x01, 0x00, 0x01, /* MP IPv4/uni */
      0x02, 0x06, 0x01, 0x04, 0x00, 0x02, 0x00, 0x01, /* MP IPv6/uni */
      0x02, 0x02, 0x80, 0x00, /* RR old */
      0x02, 0x02, 0x02, 0x00, /* RR */
      0x02, 0x06, 0x41, 0x04, 0x00, 0x03, 0x00, 0x06, /* AS4 */
    },
    32, SHOULD_PARSE, 196614,
  },
  { "AS4real2",
    "AS4 capability, in series of capabilities",
    {
      0x02, 0x06, 0x01, 0x04, 0x00, 0x01, 0x00, 0x01,
      0x02, 0x06, 0x01, 0x04, 0x00, 0x02, 0x00, 0x01,
      0x02, 0x02, 0x80, 0x00,
      0x02, 0x02, 0x02, 0x00,
      0x02, 0x06, 0x41, 0x04, 0x00, 0x00, 0xfc, 0x03,
      0x02, 0x09, 0x82, 0x07, 0x00, 0x01, 0x00, 0x01, 0x01, 0x80, 0x03,
      0x02, 0x09, 0x03, 0x07, 0x00, 0x01, 0x00, 0x01, 0x01, 0x40, 0x03,
      0x02, 0x02, 0x42, 0x00,
    },
    58, SHOULD_PARSE, 64515,
  },

  { NULL, NULL, {0}, 0, 0}
};

/* basic parsing test */
static void
parse_test (struct peer *peer, struct test_segment *t, int type)
{
  int ret;
  int capability = 0;
  as_t as4 = 0;
  int oldfailed = failed;
  int len = t->len;
#define RANDOM_FUZZ 35
  
  stream_reset (peer->ibuf);
  stream_put (peer->ibuf, NULL, RANDOM_FUZZ);
  stream_set_getp (peer->ibuf, RANDOM_FUZZ);
  
  switch (type)
    {
      case CAPABILITY:
        stream_putc (peer->ibuf, BGP_OPEN_OPT_CAP);
        stream_putc (peer->ibuf, t->len);
        break;
      case DYNCAP:
/*        for (i = 0; i < BGP_MARKER_SIZE; i++)
          stream_putc (peer->, 0xff);
        stream_putw (s, 0);
        stream_putc (s, BGP_MSG_CAPABILITY);*/
        break;
    }
  stream_write (peer->ibuf, t->data, t->len);
  
  printf ("%s: %s\n", t->name, t->desc);

  switch (type)
    {
      case CAPABILITY:
        len += 2; /* to cover the OPT-Param header */
      case OPT_PARAM:
        printf ("len: %u\n", len);
        /* peek_for_as4 wants getp at capibility*/
        as4 = peek_for_as4_capability (peer, len);
        printf ("peek_for_as4: as4 is %u\n", as4);
        /* and it should leave getp as it found it */
        assert (stream_get_getp (peer->ibuf) == RANDOM_FUZZ);
        
        ret = bgp_open_option_parse (peer, len, &capability);
        break;
      case DYNCAP:
        ret = bgp_capability_receive (peer, t->len);
        break;
      default:
        printf ("unknown type %u\n", type);
        exit(1);
    }
  
  if (!ret && t->validate_afi)
    {
      safi_t safi = t->safi;
      
      if (bgp_afi_safi_valid_indices (t->afi, &safi) != t->afi_valid)
        failed++;
      
      printf ("MP: %u/%u (%u): recv %u, nego %u\n",
              t->afi, t->safi, safi,
              peer->afc_recv[t->afi][safi],
              peer->afc_nego[t->afi][safi]);
        
      if (t->afi_valid == VALID_AFI)
        {
        
          if (!peer->afc_recv[t->afi][safi])
            failed++;
          if (!peer->afc_nego[t->afi][safi])
            failed++;
        }
    }
  
  if (as4 != t->peek_for)
    {
      printf ("as4 %u != %u\n", as4, t->peek_for);
      failed++;
    }
  
  printf ("parsed?: %s\n", ret ? "no" : "yes");
  
  if (ret != t->parses)
    failed++;
  
  if (tty)
    printf ("%s", (failed > oldfailed) ? VT100_RED "failed!" VT100_RESET 
                                         : VT100_GREEN "OK" VT100_RESET);
  else
    printf ("%s", (failed > oldfailed) ? "failed!" : "OK" );
  
  if (failed)
    printf (" (%u)", failed);
  
  printf ("\n\n");
}

static struct bgp *bgp;
static as_t asn = 100;

int
main (void)
{
  struct peer *peer;
  int i, j;
  
  conf_bgp_debug_fsm = -1UL;
  conf_bgp_debug_events = -1UL;
  conf_bgp_debug_packet = -1UL;
  conf_bgp_debug_normal = -1UL;
  conf_bgp_debug_as4 = -1UL;
  term_bgp_debug_fsm = -1UL;
  term_bgp_debug_events = -1UL;
  term_bgp_debug_packet = -1UL;
  term_bgp_debug_normal = -1UL;
  term_bgp_debug_as4 = -1UL;
  
  master = thread_master_create ();
  bgp_master_init ();
  bgp_option_set (BGP_OPT_NO_LISTEN);
  
  if (fileno (stdout) >= 0) 
    tty = isatty (fileno (stdout));
  
  if (bgp_get (&bgp, &asn, NULL))
    return -1;
  
  peer = peer_create_accept (bgp);
  peer->host = (char *) "foo";
  
  for (i = AFI_IP; i < AFI_MAX; i++)
    for (j = SAFI_UNICAST; j < SAFI_MAX; j++)
      {
        peer->afc[i][j] = 1;
        peer->afc_adv[i][j] = 1;
      }
  
  i = 0;
  while (mp_segments[i].name)
    parse_test (peer, &mp_segments[i++], CAPABILITY);

  /* These tests assume mp_segments tests set at least
   * one of the afc_nego's
   */
  i = 0;
  while (test_segments[i].name)   
    parse_test (peer, &test_segments[i++], CAPABILITY);
  
  i = 0;
  while (misc_segments[i].name)
    parse_test (peer, &misc_segments[i++], CAPABILITY);

  i = 0;
  while (opt_params[i].name)
    parse_test (peer, &opt_params[i++], OPT_PARAM);

  SET_FLAG (peer->cap, PEER_CAP_DYNAMIC_ADV);
  peer->status = Established;
  
  i = 0;
  while (dynamic_cap_msgs[i].name)
    parse_test (peer, &dynamic_cap_msgs[i++], DYNCAP);
  
  printf ("failures: %d\n", failed);
  return failed;
}
