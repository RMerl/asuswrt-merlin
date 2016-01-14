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
#include "bgpd/bgp_ecommunity.h"

/* need these to link in libbgp */
struct zebra_privs_t *bgpd_privs = NULL;
struct thread_master *master = NULL;

static int failed = 0;

/* specification for a test - what the results should be */
struct test_spec 
{
  const char *shouldbe; /* the string the path should parse to */
};


/* test segments to parse and validate, and use for other tests */
static struct test_segment {
  const char *name;
  const char *desc;
  const u_int8_t data[1024];
  int len;
  struct test_spec sp;
} test_segments [] = 
{
  { /* 0 */
    "ipaddr",
    "rt 1.2.3.4:257",
    { ECOMMUNITY_ENCODE_IP, ECOMMUNITY_ROUTE_TARGET,
      0x1,0x2,0x3,0x4,	0x1,0x1 },
    8,
    { "rt 1.2.3.4:257" }
  },
  { /* 1 */
    "ipaddr-so",
    "soo 1.2.3.4:257",
    { ECOMMUNITY_ENCODE_IP, ECOMMUNITY_SITE_ORIGIN,
      0x1,0x2,0x3,0x4,	0x1,0x1},
    8,
    { "soo 1.2.3.4:257" }
  },
  { /* 2 */
    "asn",
    "rt 23456:987654321",
    { ECOMMUNITY_ENCODE_AS, ECOMMUNITY_SITE_ORIGIN,
      0x5b,0xa0,	0x3a,0xde,0x68,0xb1 },
    8,
    { "soo 23456:987654321" }
  },
  { /* 3 */
    "asn4",
    "rt 168450976:4321",
    { ECOMMUNITY_ENCODE_AS4, ECOMMUNITY_SITE_ORIGIN,
      0xa,0xa,0x5b,0xa0,	0x10,0xe1 },
    8,
    { "soo 168450976:4321" }
  },
  { NULL, NULL, {0}, 0, { NULL } }
};


/* validate the given aspath */
static int
validate (struct ecommunity *ecom, const struct test_spec *sp)
{
  int fails = 0;
  struct ecommunity *etmp;
  char *str1, *str2;
    
  printf ("got:\n  %s\n", ecommunity_str (ecom));
  str1 = ecommunity_ecom2str (ecom, ECOMMUNITY_FORMAT_COMMUNITY_LIST);
  etmp = ecommunity_str2com (str1, 0, 1);
  if (etmp)
    str2 = ecommunity_ecom2str (etmp, ECOMMUNITY_FORMAT_COMMUNITY_LIST);
  else
    str2 = NULL;
  
  if (strcmp (sp->shouldbe, str1))
    {
      failed++;
      fails++;
      printf ("shouldbe: %s\n%s\n", str1, sp->shouldbe);
    }
  if (!etmp || strcmp (str1, str2))
    {
      failed++;
      fails++;
      printf ("dogfood: in %s\n"
              "    in->out %s\n",
              str1, 
              (etmp && str2) ? str2 : "NULL");
    }
  ecommunity_free (&etmp);
  XFREE (MTYPE_ECOMMUNITY_STR, str1);
  XFREE (MTYPE_ECOMMUNITY_STR, str2);
  
  return fails;
}

/* basic parsing test */
static void
parse_test (struct test_segment *t)
{
  struct ecommunity *ecom;
  
  printf ("%s: %s\n", t->name, t->desc);

  ecom = ecommunity_parse (t->data, t->len);

  printf ("ecom: %s\nvalidating...:\n", ecommunity_str (ecom));

  if (!validate (ecom, &t->sp))
    printf ("OK\n");
  else
    printf ("failed\n");
  
  printf ("\n");
  ecommunity_unintern (&ecom);
}

     
int
main (void)
{
  int i = 0;
  ecommunity_init();
  while (test_segments[i].name)
    parse_test (&test_segments[i++]);
  
  printf ("failures: %d\n", failed);
  //printf ("aspath count: %ld\n", aspath_count());
  return failed;
  //return (failed + aspath_count());
}
