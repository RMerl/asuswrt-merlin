/* BGP Extended Communities Attribute.
   Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* Extended Communities Transitive flag. */
#define ECOMMUNITY_FLAG_NON_TRANSITIVE      0x40  

/* High-order octet of the Extended Communities type field.  */
#define ECOMMUNITY_ENCODE_AS                0x00
#define ECOMMUNITY_ENCODE_IP                0x01
#define ECOMMUNITY_ENCODE_4OCTET_AS         0x02
#define ECOMMUNITY_ENCODE_OPAQUE            0x03

/* Low-order octet of the Extended Communityes type field.  */
#define ECOMMUNITY_TYPE_COST_COMMUNITY      0x01
#define ECOMMUNITY_TYPE_ROUTE_TARGET        0x02
#define ECOMMUNITY_TYPE_SITE_ORIGIN         0x03

/* High-order octet and Low-order octet of the Extended Communityes type field.  */
#define ECOMMUNITY_COST_COMMUNITY         0x4301

/* Extended communities attribute string format.  */
#define ECOMMUNITY_FORMAT_CONFIG               0
#define ECOMMUNITY_FORMAT_DISPLAY              1
#define ECOMMUNITY_FORMAT_RMAP                 2

/* Extended communities Cost Community */
#define ECOMMUNITY_COST_POI_IGP              129

/* Extended Communities value is eight octet long. */
#define ECOMMUNITY_SIZE                        8
 
/* Cost community default value. */
#define COST_COMMUNITY_DEFAULT_COST   0x7FFFFFFF

/* Extended Communities attribute.  */
struct ecommunity
{
  /* Reference counter.  */
  unsigned long refcnt;

  /* Size of Extended Communities attribute.  */
  int size;

  /* Extended Communities value.  */
  u_char *val;

  /* Human readable format string.  */
  char *str;
};

/* Extended community value is eight octet.  */
struct ecommunity_val
{
  char val[ECOMMUNITY_SIZE];
};

struct ecommunity_cost
{
  u_int16_t type;
  u_char poi;
  u_char id;
  u_int32_t val;
};

#define ecom_length(X)    ((X)->size * ECOMMUNITY_SIZE)

void ecommunity_init (void);
void ecommunity_free (struct ecommunity *);
struct ecommunity *ecommunity_new (void);
struct ecommunity *ecommunity_parse (char *, u_short);
struct ecommunity *ecommunity_dup (struct ecommunity *);
struct ecommunity *ecommunity_merge (struct ecommunity *, struct ecommunity *);
struct ecommunity *ecommunity_intern (struct ecommunity *);
int ecommunity_cmp (struct ecommunity *, struct ecommunity *);
void ecommunity_unintern (struct ecommunity *);
unsigned int ecommunity_hash_make (struct ecommunity *);
struct ecommunity *ecommunity_str2com (char *, int, int);
char *ecommunity_ecom2str (struct ecommunity *, int);
int ecommunity_match (struct ecommunity *, struct ecommunity *);
char *ecommunity_str (struct ecommunity *);
struct ecommunity *ecommunity_cost_str2com (char *, u_char);
int ecommunity_cost_cmp (struct ecommunity *, struct ecommunity *, u_char);
