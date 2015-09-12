/* 
 * Copyright (C) 2005 Sun Microsystems, Inc.
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

#include "bgpd/bgpd.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_attr.h"

#define VT100_RESET "\x1b[0m"
#define VT100_RED "\x1b[31m"
#define VT100_GREEN "\x1b[32m"
#define VT100_YELLOW "\x1b[33m"
#define OK VT100_GREEN "OK" VT100_RESET
#define FAILED VT100_RED "failed" VT100_RESET

/* need these to link in libbgp */
struct zebra_privs_t *bgpd_privs = NULL;
struct thread_master *master = NULL;

static int failed = 0;

/* specification for a test - what the results should be */
struct test_spec 
{
  const char *shouldbe; /* the string the path should parse to */
  const char *shouldbe_delete_confed; /* ditto, but once confeds are deleted */
  const unsigned int hops; /* aspath_count_hops result */
  const unsigned int confeds; /* aspath_count_confeds */
  const int private_as; /* whether the private_as check should pass or fail */
#define NOT_ALL_PRIVATE 0
#define ALL_PRIVATE 1
  const as_t does_loop; /* an ASN which should trigger loop-check */
  const as_t doesnt_loop; /* one which should not */
  const as_t first; /* the first ASN, if there is one */
#define NULL_ASN 0
};


/* test segments to parse and validate, and use for other tests */
static struct test_segment {
  const char *name;
  const char *desc;
  const u_char asdata[1024];
  int len;
  struct test_spec sp;
} test_segments [] = 
{
  { /* 0 */ 
    "seq1",
    "seq(8466,3,52737,4096)",
    { 0x2,0x4, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00 },
    10,
    { "8466 3 52737 4096",
      "8466 3 52737 4096",
      4, 0, NOT_ALL_PRIVATE, 4096, 4, 8466 },
  },
  { /* 1 */
    "seq2",
    "seq(8722) seq(4)",
    { 0x2,0x1, 0x22,0x12,
      0x2,0x1, 0x00,0x04 },
    8,
    { "8722 4",
      "8722 4",
      2, 0, NOT_ALL_PRIVATE, 4, 5, 8722, },
  },
  { /* 2 */
    "seq3",
    "seq(8466,3,52737,4096,8722,4)",
    { 0x2,0x6, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 
               0x22,0x12, 0x00,0x04},
    14,
    { "8466 3 52737 4096 8722 4",
      "8466 3 52737 4096 8722 4",
       6, 0, NOT_ALL_PRIVATE, 3, 5, 8466 },
  },
  { /* 3 */
    "seqset",
    "seq(8482,51457) set(5204)",
    { 0x2,0x2, 0x21,0x22, 0xc9,0x01,
      0x1,0x1, 0x14,0x54 },
    10,
    { "8482 51457 {5204}",
      "8482 51457 {5204}",
      3, 0, NOT_ALL_PRIVATE, 5204, 51456, 8482},
  },
  { /* 4 */
    "seqset2",
    "seq(8467, 59649) set(4196,48658) set(17322,30745)",
    { 0x2,0x2, 0x21,0x13, 0xe9,0x01,
      0x1,0x2, 0x10,0x64, 0xbe,0x12,
      0x1,0x2, 0x43,0xaa, 0x78,0x19 },    
    18,
    { "8467 59649 {4196,48658} {17322,30745}",
      "8467 59649 {4196,48658} {17322,30745}",
      4, 0, NOT_ALL_PRIVATE, 48658, 1, 8467},
  },
  { /* 5 */
    "multi",
    "seq(6435,59408,21665) set(2457,61697,4369), seq(1842,41590,51793)",
    { 0x2,0x3, 0x19,0x23, 0xe8,0x10, 0x54,0xa1,
      0x1,0x3, 0x09,0x99, 0xf1,0x01, 0x11,0x11,
      0x2,0x3, 0x07,0x32, 0xa2,0x76, 0xca,0x51 },
    24,
    { "6435 59408 21665 {2457,4369,61697} 1842 41590 51793",
      "6435 59408 21665 {2457,4369,61697} 1842 41590 51793",
      7, 0, NOT_ALL_PRIVATE, 51793, 1, 6435 },
  },
  { /* 6 */
    "confed",
    "confseq(123,456,789)",
    { 0x3,0x3, 0x00,0x7b, 0x01,0xc8, 0x03,0x15 },
    8,
    { "(123 456 789)",
      "",
      0, 3, NOT_ALL_PRIVATE, 789, 1, NULL_ASN },
  },
  { /* 7 */
    "confed2",
    "confseq(123,456,789) confseq(111,222)",
    { 0x3,0x3, 0x00,0x7b, 0x01,0xc8, 0x03,0x15,
      0x3,0x2, 0x00,0x6f, 0x00,0xde },
    14,
    { "(123 456 789) (111 222)",
      "",
      0, 5, NOT_ALL_PRIVATE, 111, 1, NULL_ASN },
  },
  { /* 8 */
    "confset",
    "confset(456,123,789)",
    { 0x4,0x3, 0x01,0xc8, 0x00,0x7b, 0x03,0x15 },
    8,
    { "[123,456,789]",
      "[123,456,789]",
      0, 1, NOT_ALL_PRIVATE, 123, 1, NULL_ASN },
  },
  { /* 9 */
    "confmulti",
    "confseq(123,456,789) confset(222,111) seq(8722) set(4196,48658)",
    { 0x3,0x3, 0x00,0x7b, 0x01,0xc8, 0x03,0x15,
      0x4,0x2, 0x00,0xde, 0x00,0x6f,
      0x2,0x1, 0x22,0x12,
      0x1,0x2, 0x10,0x64, 0xbe,0x12 },
    24,
    { "(123 456 789) [111,222] 8722 {4196,48658}",
      "8722 {4196,48658}",
      2, 4, NOT_ALL_PRIVATE, 123, 1, NULL_ASN },
  },
  { /* 10 */
    "seq4",
    "seq(8466,2,52737,4096,8722,4)",
    { 0x2,0x6, 0x21,0x12, 0x00,0x02, 0xce,0x01, 0x10,0x00, 
               0x22,0x12, 0x00,0x04},
    14,
    { "8466 2 52737 4096 8722 4",
      "8466 2 52737 4096 8722 4",
      6, 0, NOT_ALL_PRIVATE, 4096, 1, 8466 },
  },
  { /* 11 */
    "tripleseq1",
    "seq(8466,2,52737) seq(4096,8722,4) seq(8722)",
    { 0x2,0x3, 0x21,0x12, 0x00,0x02, 0xce,0x01, 
      0x2,0x3, 0x10,0x00, 0x22,0x12, 0x00,0x04,
      0x2,0x1, 0x22,0x12},
    20,
    { "8466 2 52737 4096 8722 4 8722",
      "8466 2 52737 4096 8722 4 8722",
      7, 0, NOT_ALL_PRIVATE, 4096, 1, 8466 },
  },
  { /* 12 */ 
    "someprivate",
    "seq(8466,64512,52737,65535)",
    { 0x2,0x4, 0x21,0x12, 0xfc,0x00, 0xce,0x01, 0xff,0xff },
    10,
    { "8466 64512 52737 65535",
      "8466 64512 52737 65535",
      4, 0, NOT_ALL_PRIVATE, 65535, 4, 8466 },
  },
  { /* 13 */ 
    "allprivate",
    "seq(65534,64512,64513,65535)",
    { 0x2,0x4, 0xff,0xfe, 0xfc,0x00, 0xfc,0x01, 0xff,0xff },
    10,
    { "65534 64512 64513 65535",
      "65534 64512 64513 65535",
      4, 0, ALL_PRIVATE, 65534, 4, 65534 },
  },
  { /* 14 */ 
    "long",
    "seq(8466,3,52737,4096,34285,<repeated 49 more times>)",
    { 0x2,0xfa, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed,
                0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x85,0xed, },
    502,
    { "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285",
      
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285",
      250, 0, NOT_ALL_PRIVATE, 4096, 4, 8466 },
  },
  { /* 15 */ 
    "seq1extra",
    "seq(8466,3,52737,4096,3456)",
    { 0x2,0x5, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x0d,0x80 },
    12,
    { "8466 3 52737 4096 3456",
      "8466 3 52737 4096 3456",
      5, 0, NOT_ALL_PRIVATE, 4096, 4, 8466 },
  },
  { /* 16 */
    "empty",
    "<empty>",
    {},
    0,
    { "", "", 0, 0, 0, 0, 0, 0 },
  },
  { /* 17 */ 
    "redundantset",
    "seq(8466,3,52737,4096,3456) set(7099,8153,8153,8153)",
    { 0x2,0x5, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x0d,0x80,
      0x1,0x4, 0x1b,0xbb, 0x1f,0xd9, 0x1f,0xd9, 0x1f,0xd9 },
    22,
    {
     /* We shouldn't ever /generate/ such paths. However, we should
      * cope with them fine.
      */
     "8466 3 52737 4096 3456 {7099,8153}",
      "8466 3 52737 4096 3456 {7099,8153}",
      6, 0, NOT_ALL_PRIVATE, 4096, 4, 8466 },
  },
  { /* 18 */
    "reconcile_lead_asp",
    "seq(6435,59408,21665) set(23456,23456,23456), seq(23456,23456,23456)",
    { 0x2,0x3, 0x19,0x23, 0xe8,0x10, 0x54,0xa1,
      0x1,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0,
      0x2,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0 },
    24,
    { "6435 59408 21665 {23456} 23456 23456 23456",
      "6435 59408 21665 {23456} 23456 23456 23456",
      7, 0, NOT_ALL_PRIVATE, 23456, 1, 6435 },
  },
  { /* 19 */
    "reconcile_new_asp",
    "set(2457,61697,4369), seq(1842,41591,51793)",
    { 
      0x1,0x3, 0x09,0x99, 0xf1,0x01, 0x11,0x11,
      0x2,0x3, 0x07,0x32, 0xa2,0x77, 0xca,0x51 },
    16,
    { "{2457,4369,61697} 1842 41591 51793",
      "{2457,4369,61697} 1842 41591 51793",
      4, 0, NOT_ALL_PRIVATE, 51793, 1, 2457 },
  },
  { /* 20 */
    "reconcile_confed",
    "confseq(123,456,789) confset(456,124,788) seq(6435,59408,21665)"
    " set(23456,23456,23456), seq(23456,23456,23456)",
    { 0x3,0x3, 0x00,0x7b, 0x01,0xc8, 0x03,0x15,
      0x4,0x3, 0x01,0xc8, 0x00,0x7c, 0x03,0x14,
      0x2,0x3, 0x19,0x23, 0xe8,0x10, 0x54,0xa1,
      0x1,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0,
      0x2,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0 },
    40,
    { "(123 456 789) [124,456,788] 6435 59408 21665"
      " {23456} 23456 23456 23456",
      "6435 59408 21665 {23456} 23456 23456 23456",
      7, 4, NOT_ALL_PRIVATE, 23456, 1, 6435 },
  },
  { /* 21 */
    "reconcile_start_trans",
    "seq(23456,23456,23456) seq(6435,59408,21665)",
    { 0x2,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0,
      0x2,0x3, 0x19,0x23, 0xe8,0x10, 0x54,0xa1, },
    16,
    { "23456 23456 23456 6435 59408 21665",
      "23456 23456 23456 6435 59408 21665",
      6, 0, NOT_ALL_PRIVATE, 21665, 1, 23456 },
  },
  { /* 22 */
    "reconcile_start_trans4",
    "seq(1842,41591,51793) seq(6435,59408,21665)",
    { 0x2,0x3, 0x07,0x32, 0xa2,0x77, 0xca,0x51,
      0x2,0x3, 0x19,0x23, 0xe8,0x10, 0x54,0xa1, },
    16,
    { "1842 41591 51793 6435 59408 21665",
      "1842 41591 51793 6435 59408 21665",
      6, 0, NOT_ALL_PRIVATE, 41591, 1, 1842 },
  },
  { /* 23 */
    "reconcile_start_trans_error",
    "seq(23456,23456,23456) seq(6435,59408)",
    { 0x2,0x3, 0x5b,0xa0, 0x5b,0xa0, 0x5b,0xa0,
      0x2,0x2, 0x19,0x23, 0xe8,0x10, },
    14,
    { "23456 23456 23456 6435 59408",
      "23456 23456 23456 6435 59408",
      5, 0, NOT_ALL_PRIVATE, 59408, 1, 23456 },
  },
  { /* 24 */ 
    "redundantset2",
    "seq(8466,3,52737,4096,3456) set(7099,8153,8153,8153,7099)",
    { 0x2,0x5, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x0d,0x80,
      0x1,0x5, 0x1b,0xbb, 0x1f,0xd9, 0x1f,0xd9, 0x1f,0xd9, 0x1b,0xbb,},
    24,
    {
     /* We should weed out duplicate set members. */
     "8466 3 52737 4096 3456 {7099,8153}",
      "8466 3 52737 4096 3456 {7099,8153}",
      6, 0, NOT_ALL_PRIVATE, 4096, 4, 8466 },
  },
  { /* 25 */ 
    "zero-size overflow",
    "#ASNs = 0, data = seq(8466 3 52737 4096 3456)",
    { 0x2,0x0, 0x21,0x12, 0x00,0x03, 0xce,0x01, 0x10,0x00, 0x0d,0x80 },
    12,
    { NULL, NULL,
      0, 0, 0, 0, 0, 0 },
  },
  { /* 26  */ 
    "zero-size overflow + valid segment",
    "seq(#AS=0:8466 3 52737),seq(4096 3456)",
    { 0x2,0x0, 0x21,0x12, 0x00,0x03, 0xce,0x01, 
      0x2,0x2, 0x10,0x00, 0x0d,0x80 },
    14
    ,
    { NULL, NULL,
      0, 0, 0, 0, 0, 0 },
  },
  { /* 27  */ 
    "invalid segment type",
    "type=8(4096 3456)",
    { 0x8,0x2, 0x10,0x00, 0x0d,0x80 },
    14
    ,
    { NULL, NULL,
      0, 0, 0, 0, 0, 0 },
  },  { NULL, NULL, {0}, 0, { NULL, 0, 0 } }
};

#define COMMON_ATTRS \
      BGP_ATTR_FLAG_TRANS, \
      BGP_ATTR_ORIGIN, \
      1, \
      BGP_ORIGIN_EGP, \
      BGP_ATTR_FLAG_TRANS, \
      BGP_ATTR_NEXT_HOP, \
      4, 192, 0, 2, 0
#define COMMON_ATTR_SIZE 11

/* */
static struct aspath_tests {
  const char *desc;
  const struct test_segment *segment;
  const char *shouldbe;  /* String it should evaluate to */
  const enum as4 { AS4_DATA, AS2_DATA }
          as4; 	/* whether data should be as4 or not (ie as2) */
  const int result;	/* expected result for bgp_attr_parse */
  const int cap;	/* capabilities to set for peer */
  const char attrheader [1024];
  size_t len;
  const struct test_segment *old_segment;
} aspath_tests [] =
{
  /* 0 */
  {
    "basic test",
    &test_segments[0],
    "8466 3 52737 4096",
    AS2_DATA, 0,
    0,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH,
      10,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 1 */
  {
    "length too short",
    &test_segments[0],
    "8466 3 52737 4096",
    AS2_DATA, -1,
    0,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      8,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 2 */
  {
    "length too long",
    &test_segments[0],
    "8466 3 52737 4096",
    AS2_DATA, -1,
    0,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      12,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 3 */
  {
    "incorrect flag",
    &test_segments[0],
    "8466 3 52737 4096",
    AS2_DATA, -1,
    0,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS_PATH, 
      10,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 4 */
  {
    "as4_path, with as2 format data",
    &test_segments[0],
    "8466 3 52737 4096",
    AS2_DATA, -1,
    0,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS4_PATH, 
      10,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 5 */
  {
    "as4, with incorrect attr length",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, -1,
    PEER_CAP_AS4_RCV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS4_PATH, 
      10,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 6 */
  {
    "basic 4-byte as-path",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, 0,
    PEER_CAP_AS4_RCV|PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      18,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 7 */
  {
    "4b AS_PATH: too short",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, -1,
    PEER_CAP_AS4_RCV|PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      16,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 8 */
  {
    "4b AS_PATH: too long",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, -1,
    PEER_CAP_AS4_RCV|PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      20,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 9 */
  {
    "4b AS_PATH: too long2",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, -1,
    PEER_CAP_AS4_RCV|PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS,
      BGP_ATTR_AS_PATH, 
      22,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 10 */
  {
    "4b AS_PATH: bad flags",
    &test_segments[0],
    "8466 3 52737 4096",
    AS4_DATA, -1,
    PEER_CAP_AS4_RCV|PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS_PATH, 
      18,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 11 */
  {
    "4b AS4_PATH w/o AS_PATH",
    &test_segments[6],
    NULL,
    AS4_DATA, -1,
    PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS4_PATH, 
      14,
    },
    COMMON_ATTR_SIZE + 3,
  },
  /* 12 */
  {
    "4b AS4_PATH: confed",
    &test_segments[6],
    "8466 3 52737 4096 (123 456 789)",
    AS4_DATA, 0,
    PEER_CAP_AS4_ADV,
    { COMMON_ATTRS,
      BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_OPTIONAL,
      BGP_ATTR_AS4_PATH, 
      14,
    },
    COMMON_ATTR_SIZE + 3,
    &test_segments[0],
  },
  { NULL, NULL, NULL, 0, 0, 0, { 0 }, 0 },
};

/* prepending tests */
static struct tests {
  const struct test_segment *test1;
  const struct test_segment *test2;
  struct test_spec sp;
} prepend_tests[] = 
{
  /* 0 */
  { &test_segments[0], &test_segments[1],
    { "8466 3 52737 4096 8722 4",
      "8466 3 52737 4096 8722 4",
      6, 0, NOT_ALL_PRIVATE, 4096, 1, 8466 },
  },
  /* 1 */
  { &test_segments[1], &test_segments[3],
    { "8722 4 8482 51457 {5204}",
      "8722 4 8482 51457 {5204}",
      5, 0, NOT_ALL_PRIVATE, 5204, 1, 8722 }
  },
  /* 2 */
  { &test_segments[3], &test_segments[4],
    { "8482 51457 {5204} 8467 59649 {4196,48658} {17322,30745}",
      "8482 51457 {5204} 8467 59649 {4196,48658} {17322,30745}",
      7, 0, NOT_ALL_PRIVATE, 5204, 1, 8482 },
  },
  /* 3 */
  { &test_segments[4], &test_segments[5],
    { "8467 59649 {4196,48658} {17322,30745} 6435 59408 21665"
      " {2457,4369,61697} 1842 41590 51793",
      "8467 59649 {4196,48658} {17322,30745} 6435 59408 21665"
      " {2457,4369,61697} 1842 41590 51793",
      11, 0, NOT_ALL_PRIVATE, 61697, 1, 8467 }
  },
  /* 4 */
  { &test_segments[5], &test_segments[6],
    { "6435 59408 21665 {2457,4369,61697} 1842 41590 51793",
      "6435 59408 21665 {2457,4369,61697} 1842 41590 51793",
      7, 0, NOT_ALL_PRIVATE, 1842, 1, 6435 },
  },
  /* 5 */
  { &test_segments[6], &test_segments[7],
    { "(123 456 789) (123 456 789) (111 222)",
      "",
      0, 8, NOT_ALL_PRIVATE, 111, 1, 0 }
  },
  { &test_segments[7], &test_segments[8],
    { "(123 456 789) (111 222) [123,456,789]",
      "",
      0, 6, NOT_ALL_PRIVATE, 111, 1, 0 }
  },
  { &test_segments[8], &test_segments[9],
    { "[123,456,789] (123 456 789) [111,222] 8722 {4196,48658}",
      "[123,456,789] (123 456 789) [111,222] 8722 {4196,48658}",
      2, 5, NOT_ALL_PRIVATE, 456, 1, NULL_ASN },
  },
  { &test_segments[9], &test_segments[8],
    { "(123 456 789) [111,222] 8722 {4196,48658} [123,456,789]",
      "8722 {4196,48658} [123,456,789]",
      2, 5, NOT_ALL_PRIVATE, 48658, 1, NULL_ASN },
  },
  { &test_segments[14], &test_segments[11],
    { "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 2 52737 4096 8722 4 8722",
      
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 3 52737 4096 34285 8466 3 52737 4096 34285 "
      "8466 2 52737 4096 8722 4 8722",
      257, 0, NOT_ALL_PRIVATE, 4096, 1000, 8466 },
  },
  { NULL, NULL, { NULL, 0, 0, 0, 0, 0, 0, } },
};

struct tests reconcile_tests[] =
{
  { &test_segments[18], &test_segments[19],
    { "6435 59408 21665 {2457,4369,61697} 1842 41591 51793",
      "6435 59408 21665 {2457,4369,61697} 1842 41591 51793",
      7, 0, NOT_ALL_PRIVATE, 51793, 1, 6435 },
  },
  { &test_segments[19], &test_segments[18],
    /* AS_PATH (19) has more hops than NEW_AS_PATH,
     * so just AS_PATH should be used (though, this practice
     * is bad imho).
     */
    { "{2457,4369,61697} 1842 41591 51793 6435 59408 21665 {23456} 23456 23456 23456",
      "{2457,4369,61697} 1842 41591 51793 6435 59408 21665 {23456} 23456 23456 23456",
      11, 0, NOT_ALL_PRIVATE, 51793, 1, 6435 },
  },
  { &test_segments[20], &test_segments[19],
    { "(123 456 789) [124,456,788] 6435 59408 21665"
      " {2457,4369,61697} 1842 41591 51793",
      "6435 59408 21665 {2457,4369,61697} 1842 41591 51793",
      7, 4, NOT_ALL_PRIVATE, 51793, 1, 6435 },
  },
  { &test_segments[21], &test_segments[22],
    { "1842 41591 51793 6435 59408 21665",
      "1842 41591 51793 6435 59408 21665",
      6, 0, NOT_ALL_PRIVATE, 51793, 1, 1842 },
  },
  { &test_segments[23], &test_segments[22],
    { "23456 23456 23456 6435 59408 1842 41591 51793 6435 59408 21665",
      "23456 23456 23456 6435 59408 1842 41591 51793 6435 59408 21665",
      11, 0, NOT_ALL_PRIVATE, 51793, 1, 1842 },
  },
  { NULL, NULL, { NULL, 0, 0, 0, 0, 0, 0, } },
};
  
struct tests aggregate_tests[] =
{
  { &test_segments[0], &test_segments[2],
    { "8466 3 52737 4096 {4,8722}",
      "8466 3 52737 4096 {4,8722}",
      5, 0, NOT_ALL_PRIVATE, 4, 1, 8466 },
  },
  { &test_segments[2], &test_segments[0],
    { "8466 3 52737 4096 {4,8722}",
      "8466 3 52737 4096 {4,8722}",
      5, 0, NOT_ALL_PRIVATE, 8722, 1, 8466 },
  },
  { &test_segments[2], &test_segments[10],
    { "8466 {2,3,4,4096,8722,52737}",
      "8466 {2,3,4,4096,8722,52737}",
      2, 0, NOT_ALL_PRIVATE, 8722, 5, 8466 },
  },
  { &test_segments[10], &test_segments[2],
    { "8466 {2,3,4,4096,8722,52737}",
      "8466 {2,3,4,4096,8722,52737}",
      2, 0, NOT_ALL_PRIVATE, 2, 20000, 8466 },
  },

  { &test_segments[5], &test_segments[18],
    { "6435 59408 21665 {1842,2457,4369,23456,41590,51793,61697}",
      "6435 59408 21665 {1842,2457,4369,23456,41590,51793,61697}",
      4, 0, NOT_ALL_PRIVATE, 41590, 1, 6435 },
  },

  { NULL, NULL, { NULL, 0, 0}  },
};

struct compare_tests 
{
  int test_index1;
  int test_index2;
#define CMP_RES_YES 1
#define CMP_RES_NO 0
  char shouldbe_cmp;
  char shouldbe_confed;
} left_compare [] =
{
  { 0, 1, CMP_RES_NO, CMP_RES_NO },
  { 0, 2, CMP_RES_YES, CMP_RES_NO },
  { 0, 11, CMP_RES_YES, CMP_RES_NO },
  { 0, 15, CMP_RES_YES, CMP_RES_NO },
  { 0, 16, CMP_RES_NO, CMP_RES_NO },
  { 1, 11, CMP_RES_NO, CMP_RES_NO },
  { 6, 7, CMP_RES_NO, CMP_RES_YES },
  { 6, 8, CMP_RES_NO, CMP_RES_NO },
  { 7, 8, CMP_RES_NO, CMP_RES_NO },
  { 1, 9, CMP_RES_YES, CMP_RES_NO },
  { 0, 9, CMP_RES_NO, CMP_RES_NO },
  { 3, 9, CMP_RES_NO, CMP_RES_NO },
  { 0, 6, CMP_RES_NO, CMP_RES_NO },
  { 1, 6, CMP_RES_NO, CMP_RES_NO },
  { 0, 8, CMP_RES_NO, CMP_RES_NO },
  { 1, 8, CMP_RES_NO, CMP_RES_NO },
  { 11, 6, CMP_RES_NO, CMP_RES_NO },
  { 11, 7, CMP_RES_NO, CMP_RES_NO },
  { 11, 8, CMP_RES_NO, CMP_RES_NO },
  { 9, 6, CMP_RES_NO, CMP_RES_YES },
  { 9, 7, CMP_RES_NO, CMP_RES_YES },
  { 9, 8, CMP_RES_NO, CMP_RES_NO },
};

/* make an aspath from a data stream */
static struct aspath *
make_aspath (const u_char *data, size_t len, int use32bit)
{
  struct stream *s = NULL;
  struct aspath *as;
  
  if (len)
    {
      s = stream_new (len);
      stream_put (s, data, len);
    }
  as = aspath_parse (s, len, use32bit);
  
  if (s)
    stream_free (s);
  
  return as;
}

static void
printbytes (const u_char *bytes, int len)
{
  int i = 0;
  while (i < len)
    {
      if (i % 2)
        printf ("%02hhx%s", bytes[i], " ");
      else
        printf ("0x%02hhx", bytes[i]);
      i++;
    }
  printf ("\n");
}  

/* validate the given aspath */
static int
validate (struct aspath *as, const struct test_spec *sp)
{
  size_t bytes, bytes4;
  int fails = 0;
  const u_char *out;
  static struct stream *s;
  struct aspath *asinout, *asconfeddel, *asstr, *as4;
  
  if (as == NULL && sp->shouldbe == NULL)
    {
      printf ("Correctly failed to parse\n");
      return fails;
    }
  
  out = aspath_snmp_pathseg (as, &bytes);
  asinout = make_aspath (out, bytes, 0);
  
  /* Excercise AS4 parsing a bit, with a dogfood test */
  if (!s)
    s = stream_new (4096);
  bytes4 = aspath_put (s, as, 1);
  as4 = make_aspath (STREAM_DATA(s), bytes4, 1);
  
  asstr = aspath_str2aspath (sp->shouldbe);
  
  asconfeddel = aspath_delete_confed_seq (aspath_dup (asinout));
  
  printf ("got: %s\n", aspath_print(as));
  
  /* the parsed path should match the specified 'shouldbe' string.
   * We should pass the "eat our own dog food" test, be able to output
   * this path and then input it again. Ie the path resulting from:
   *
   *   aspath_parse(aspath_put(as)) 
   *
   * should:
   *
   * - also match the specified 'shouldbe' value
   * - hash to same value as original path
   * - have same hops and confed counts as original, and as the
   *   the specified counts
   *
   * aspath_str2aspath() and shouldbe should match
   *
   * We do the same for:
   *
   *   aspath_parse(aspath_put(as,USE32BIT))
   *
   * Confederation related tests: 
   * - aspath_delete_confed_seq(aspath) should match shouldbe_confed
   * - aspath_delete_confed_seq should be idempotent.
   */
  if (strcmp(aspath_print (as), sp->shouldbe)
         /* hash validation */
      || (aspath_key_make (as) != aspath_key_make (asinout))
         /* by string */
      || strcmp(aspath_print (asinout), sp->shouldbe)
         /* By 4-byte parsing */
      || strcmp(aspath_print (as4), sp->shouldbe)
         /* by various path counts */
      || (aspath_count_hops (as) != sp->hops)
      || (aspath_count_confeds (as) != sp->confeds)
      || (aspath_count_hops (asinout) != sp->hops)
      || (aspath_count_confeds (asinout) != sp->confeds))
    {
      failed++;
      fails++;
      printf ("shouldbe:\n%s\n", sp->shouldbe);
      printf ("as4:\n%s\n", aspath_print (as4));
      printf ("hash keys: in: %d out->in: %d\n", 
              aspath_key_make (as), aspath_key_make (asinout));
      printf ("hops: %d, counted %d %d\n", sp->hops, 
              aspath_count_hops (as),
              aspath_count_hops (asinout) );
      printf ("confeds: %d, counted %d %d\n", sp->confeds,
              aspath_count_confeds (as),
              aspath_count_confeds (asinout));
      printf ("out->in:\n%s\nbytes: ", aspath_print(asinout));
      printbytes (out, bytes);
    }
         /* basic confed related tests */
  if ((aspath_print (asconfeddel) == NULL 
          && sp->shouldbe_delete_confed != NULL)
      || (aspath_print (asconfeddel) != NULL 
          && sp->shouldbe_delete_confed == NULL)
      || strcmp(aspath_print (asconfeddel), sp->shouldbe_delete_confed)
         /* delete_confed_seq should be idempotent */
      || (aspath_key_make (asconfeddel) 
          != aspath_key_make (aspath_delete_confed_seq (asconfeddel))))
    {
      failed++;
      fails++;
      printf ("confed_del: %s\n", aspath_print (asconfeddel));
      printf ("should be: %s\n", sp->shouldbe_delete_confed);
    }
      /* aspath_str2aspath test */
  if ((aspath_print (asstr) == NULL && sp->shouldbe != NULL)
      || (aspath_print (asstr) != NULL && sp->shouldbe == NULL)
      || strcmp(aspath_print (asstr), sp->shouldbe))
    {
      failed++;
      fails++;
      printf ("asstr: %s\n", aspath_print (asstr));
    }
  
    /* loop, private and first as checks */
  if ((sp->does_loop && aspath_loop_check (as, sp->does_loop) == 0)
      || (sp->doesnt_loop && aspath_loop_check (as, sp->doesnt_loop) != 0)
      || (aspath_private_as_check (as) != sp->private_as)
      || (aspath_firstas_check (as,sp->first)
          && sp->first == 0))
    {
      failed++;
      fails++;
      printf ("firstas: %d,  got %d\n", sp->first,
              aspath_firstas_check (as,sp->first));
      printf ("loop does: %d %d, doesnt: %d %d\n",
              sp->does_loop, aspath_loop_check (as, sp->does_loop),
              sp->doesnt_loop, aspath_loop_check (as, sp->doesnt_loop));
      printf ("private check: %d %d\n", sp->private_as,
              aspath_private_as_check (as));
    }
  aspath_unintern (&asinout);
  aspath_unintern (&as4);
  
  aspath_free (asconfeddel);
  aspath_free (asstr);
  stream_reset (s);
  
  return fails;
}

static void
empty_get_test ()
{
  struct aspath *as = aspath_empty_get ();
  struct test_spec sp = { "", "", 0, 0, 0, 0, 0, 0 };

  printf ("empty_get_test, as: %s\n",aspath_print (as));
  if (!validate (as, &sp))
    printf ("%s\n", OK);
  else
    printf ("%s!\n", FAILED);
  
  printf ("\n");
  
  aspath_free (as);
}

/* basic parsing test */
static void
parse_test (struct test_segment *t)
{
  struct aspath *asp;
  
  printf ("%s: %s\n", t->name, t->desc);

  asp = make_aspath (t->asdata, t->len, 0);
  
  printf ("aspath: %s\nvalidating...:\n", aspath_print (asp));

  if (!validate (asp, &t->sp))
    printf (OK "\n");
  else
    printf (FAILED "\n");
  
  printf ("\n");
  
  if (asp)
    aspath_unintern (&asp);
}

/* prepend testing */
static void
prepend_test (struct tests *t)
{
  struct aspath *asp1, *asp2, *ascratch;
  
  printf ("prepend %s: %s\n", t->test1->name, t->test1->desc);
  printf ("to %s: %s\n", t->test2->name, t->test2->desc);
  
  asp1 = make_aspath (t->test1->asdata, t->test1->len, 0);
  asp2 = make_aspath (t->test2->asdata, t->test2->len, 0);
  
  ascratch = aspath_dup (asp2);
  aspath_unintern (&asp2);
  
  asp2 = aspath_prepend (asp1, ascratch);
  
  printf ("aspath: %s\n", aspath_print (asp2));
  
  if (!validate (asp2, &t->sp))
    printf ("%s\n", OK);
  else
    printf ("%s!\n", FAILED);
  
  printf ("\n");
  aspath_unintern (&asp1);
  aspath_free (asp2);
}

/* empty-prepend testing */
static void
empty_prepend_test (struct test_segment *t)
{
  struct aspath *asp1, *asp2, *ascratch;
  
  printf ("empty prepend %s: %s\n", t->name, t->desc);
  
  asp1 = make_aspath (t->asdata, t->len, 0);
  asp2 = aspath_empty ();
  
  ascratch = aspath_dup (asp2);
  aspath_unintern (&asp2);
  
  asp2 = aspath_prepend (asp1, ascratch);
  
  printf ("aspath: %s\n", aspath_print (asp2));
  
  if (!validate (asp2, &t->sp))
    printf (OK "\n");
  else
    printf (FAILED "!\n");
  
  printf ("\n");
  if (asp1)
    aspath_unintern (&asp1);
  aspath_free (asp2);
}

/* as2+as4 reconciliation testing */
static void
as4_reconcile_test (struct tests *t)
{
  struct aspath *asp1, *asp2, *ascratch;
  
  printf ("reconciling %s:\n  %s\n", t->test1->name, t->test1->desc);
  printf ("with %s:\n  %s\n", t->test2->name, t->test2->desc);
  
  asp1 = make_aspath (t->test1->asdata, t->test1->len, 0);
  asp2 = make_aspath (t->test2->asdata, t->test2->len, 0);
  
  ascratch = aspath_reconcile_as4 (asp1, asp2);
  
  if (!validate (ascratch, &t->sp))
    printf (OK "\n");
  else
    printf (FAILED "!\n");
  
  printf ("\n");
  aspath_unintern (&asp1);
  aspath_unintern (&asp2);
  aspath_free (ascratch);
}


/* aggregation testing */
static void
aggregate_test (struct tests *t)
{
  struct aspath *asp1, *asp2, *ascratch;
  
  printf ("aggregate %s: %s\n", t->test1->name, t->test1->desc);
  printf ("with %s: %s\n", t->test2->name, t->test2->desc);
  
  asp1 = make_aspath (t->test1->asdata, t->test1->len, 0);
  asp2 = make_aspath (t->test2->asdata, t->test2->len, 0);
  
  ascratch = aspath_aggregate (asp1, asp2);
  
  if (!validate (ascratch, &t->sp))
    printf (OK "\n");
  else
    printf (FAILED "!\n");
  
  printf ("\n");
  aspath_unintern (&asp1);
  aspath_unintern (&asp2);
  aspath_free (ascratch);
/*  aspath_unintern (ascratch);*/
}

/* cmp_left tests  */
static void
cmp_test ()
{
  unsigned int i;
#define CMP_TESTS_MAX \
  (sizeof(left_compare) / sizeof (struct compare_tests))

  for (i = 0; i < CMP_TESTS_MAX; i++)
    {
      struct test_segment *t1 = &test_segments[left_compare[i].test_index1];
      struct test_segment *t2 = &test_segments[left_compare[i].test_index2];
      struct aspath *asp1, *asp2;
      
      printf ("left cmp %s: %s\n", t1->name, t1->desc);
      printf ("and %s: %s\n", t2->name, t2->desc);
      
      asp1 = make_aspath (t1->asdata, t1->len, 0);
      asp2 = make_aspath (t2->asdata, t2->len, 0);
      
      if (aspath_cmp_left (asp1, asp2) != left_compare[i].shouldbe_cmp
          || aspath_cmp_left (asp2, asp1) != left_compare[i].shouldbe_cmp
          || aspath_cmp_left_confed (asp1, asp2) 
               != left_compare[i].shouldbe_confed
          || aspath_cmp_left_confed (asp2, asp1) 
               != left_compare[i].shouldbe_confed)
        {
          failed++;
          printf (FAILED "\n");
          printf ("result should be: cmp: %d, confed: %d\n", 
                  left_compare[i].shouldbe_cmp,
                  left_compare[i].shouldbe_confed);
          printf ("got: cmp %d, cmp_confed: %d\n",
                  aspath_cmp_left (asp1, asp2),
                  aspath_cmp_left_confed (asp1, asp2));
          printf("path1: %s\npath2: %s\n", aspath_print (asp1),
                 aspath_print (asp2));
        }
      else
        printf (OK "\n");
      
      printf ("\n");
      aspath_unintern (&asp1);
      aspath_unintern (&asp2);
    }
}

static int
handle_attr_test (struct aspath_tests *t)
{
  struct bgp bgp = { 0 }; 
  struct peer peer = { 0 };
  struct attr attr = { 0 };  
  int ret;
  int initfail = failed;
  struct aspath *asp;
  size_t datalen;
  
  asp = make_aspath (t->segment->asdata, t->segment->len, 0);
    
  peer.ibuf = stream_new (BGP_MAX_PACKET_SIZE);
  peer.obuf = stream_fifo_new ();
  peer.bgp = &bgp;
  peer.host = (char *)"none";
  peer.fd = -1;
  peer.cap = t->cap;
  
  stream_write (peer.ibuf, t->attrheader, t->len);
  datalen = aspath_put (peer.ibuf, asp, t->as4 == AS4_DATA);
  if (t->old_segment)
    {
      char dummyaspath[] = { BGP_ATTR_FLAG_TRANS, BGP_ATTR_AS_PATH,
                             t->old_segment->len };
      stream_write (peer.ibuf, dummyaspath, sizeof (dummyaspath));
      stream_write (peer.ibuf, t->old_segment->asdata, t->old_segment->len);
      datalen += sizeof (dummyaspath) + t->old_segment->len;
    }
  
  ret = bgp_attr_parse (&peer, &attr, t->len + datalen, NULL, NULL);
  
  if (ret != t->result)
    {
      printf ("bgp_attr_parse returned %d, expected %d\n", ret, t->result);
      printf ("datalen %zd\n", datalen);
      failed++;
    }
  if (ret != 0)
    goto out;
  
  if (t->shouldbe && attr.aspath == NULL)
    {
      printf ("aspath is NULL, but should be: %s\n", t->shouldbe);
      failed++;
    }
  if (t->shouldbe && attr.aspath && strcmp (attr.aspath->str, t->shouldbe))
    {
      printf ("attr str and 'shouldbe' mismatched!\n"
              "attr str:  %s\n"
              "shouldbe:  %s\n",
              attr.aspath->str, t->shouldbe);
      failed++;
    }
  if (!t->shouldbe && attr.aspath)
    {
      printf ("aspath should be NULL, but is: %s\n", attr.aspath->str);
      failed++;
    }

out:
  if (attr.aspath)
    aspath_unintern (&attr.aspath);
  if (asp)
    aspath_unintern (&asp);
  return failed - initfail;
}

static void
attr_test (struct aspath_tests *t)
{
    printf ("%s\n", t->desc);
    printf ("%s\n\n", handle_attr_test (t) ? FAILED : OK);  
}

int
main (void)
{
  int i = 0;
  bgp_master_init ();
  master = bm->master;
  bgp_option_set (BGP_OPT_NO_LISTEN);
  bgp_attr_init ();
  
  while (test_segments[i].name)
    {
      printf ("test %u\n", i);
      parse_test (&test_segments[i]);
      empty_prepend_test (&test_segments[i++]);
    }
  
  i = 0;
  while (prepend_tests[i].test1)
    {
      printf ("prepend test %u\n", i);
      prepend_test (&prepend_tests[i++]);
    }
  
  i = 0;
  while (aggregate_tests[i].test1)
    {
      printf ("aggregate test %u\n", i);
      aggregate_test (&aggregate_tests[i++]);
    }
  
  i = 0;
  
  while (reconcile_tests[i].test1)
    {
      printf ("reconcile test %u\n", i);
      as4_reconcile_test (&reconcile_tests[i++]);
    }
  
  i = 0;
  
  cmp_test();
  
  i = 0;
  
  empty_get_test();
  
  i = 0;
  
  while (aspath_tests[i].desc)
    {
      printf ("aspath_attr test %d\n", i);
      attr_test (&aspath_tests[i++]);
    }
  
  printf ("failures: %d\n", failed);
  printf ("aspath count: %ld\n", aspath_count());
  
  return (failed + aspath_count());
}
