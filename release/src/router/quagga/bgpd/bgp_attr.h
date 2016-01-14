/* BGP attributes. 
   Copyright (C) 1996, 97, 98 Kunihiro Ishiguro

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

#ifndef _QUAGGA_BGP_ATTR_H
#define _QUAGGA_BGP_ATTR_H

/* Simple bit mapping. */
#define BITMAP_NBBY 8

#define SET_BITMAP(MAP, NUM) \
        SET_FLAG (MAP[(NUM) / BITMAP_NBBY], 1 << ((NUM) % BITMAP_NBBY))

#define CHECK_BITMAP(MAP, NUM) \
        CHECK_FLAG (MAP[(NUM) / BITMAP_NBBY], 1 << ((NUM) % BITMAP_NBBY))

#define BGP_MED_MAX UINT32_MAX


/* BGP Attribute type range. */
#define BGP_ATTR_TYPE_RANGE     256
#define BGP_ATTR_BITMAP_SIZE    (BGP_ATTR_TYPE_RANGE / BITMAP_NBBY)

/* BGP Attribute flags. */
#define BGP_ATTR_FLAG_OPTIONAL  0x80	/* Attribute is optional. */
#define BGP_ATTR_FLAG_TRANS     0x40	/* Attribute is transitive. */
#define BGP_ATTR_FLAG_PARTIAL   0x20	/* Attribute is partial. */
#define BGP_ATTR_FLAG_EXTLEN    0x10	/* Extended length flag. */

/* BGP attribute header must bigger than 2. */
#define BGP_ATTR_MIN_LEN        3       /* Attribute flag, type length. */
#define BGP_ATTR_DEFAULT_WEIGHT 32768

/* Additional/uncommon BGP attributes.
 * lazily allocated as and when a struct attr
 * requires it.
 */
struct attr_extra
{
  /* Multi-Protocol Nexthop, AFI IPv6 */
#ifdef HAVE_IPV6
  struct in6_addr mp_nexthop_global;
  struct in6_addr mp_nexthop_local;
#endif /* HAVE_IPV6 */

  /* Extended Communities attribute. */
  struct ecommunity *ecommunity;
  
  /* Route-Reflector Cluster attribute */
  struct cluster_list *cluster;
  
  /* Unknown transitive attribute. */
  struct transit *transit;

  struct in_addr mp_nexthop_global_in;
  struct in_addr mp_nexthop_local_in;
  
  /* Aggregator Router ID attribute */
  struct in_addr aggregator_addr;
  
  /* Route Reflector Originator attribute */
  struct in_addr originator_id;
  
  /* Local weight, not actually an attribute */
  u_int32_t weight;
  
  /* Aggregator ASN */
  as_t aggregator_as;
  
  /* MP Nexthop length */
  u_char mp_nexthop_len;
};

/* BGP core attribute structure. */
struct attr
{
  /* AS Path structure */
  struct aspath *aspath;

  /* Community structure */
  struct community *community;	
  
  /* Lazily allocated pointer to extra attributes */
  struct attr_extra *extra;
  
  /* Reference count of this attribute. */
  unsigned long refcnt;

  /* Flag of attribute is set or not. */
  u_int32_t flag;
  
  /* Apart from in6_addr, the remaining static attributes */
  struct in_addr nexthop;
  u_int32_t med;
  u_int32_t local_pref;
  
  /* Path origin attribute */
  u_char origin;
};

/* Router Reflector related structure. */
struct cluster_list
{
  unsigned long refcnt;
  int length;
  struct in_addr *list;
};

/* Unknown transit attribute. */
struct transit
{
  unsigned long refcnt;
  int length;
  u_char *val;
};

#define ATTR_FLAG_BIT(X)  (1 << ((X) - 1))

typedef enum {
 BGP_ATTR_PARSE_PROCEED = 0,
 BGP_ATTR_PARSE_ERROR = -1,
 BGP_ATTR_PARSE_WITHDRAW = -2,

 /* only used internally, send notify + convert to BGP_ATTR_PARSE_ERROR */
 BGP_ATTR_PARSE_ERROR_NOTIFYPLS = -3,
} bgp_attr_parse_ret_t;

/* Prototypes. */
extern void bgp_attr_init (void);
extern void bgp_attr_finish (void);
extern bgp_attr_parse_ret_t bgp_attr_parse (struct peer *, struct attr *,
                                           bgp_size_t, struct bgp_nlri *,
                                           struct bgp_nlri *);
extern struct attr_extra *bgp_attr_extra_get (struct attr *);
extern void bgp_attr_extra_free (struct attr *);
extern void bgp_attr_dup (struct attr *, struct attr *);
extern struct attr *bgp_attr_intern (struct attr *attr);
extern void bgp_attr_unintern_sub (struct attr *);
extern void bgp_attr_unintern (struct attr **);
extern void bgp_attr_flush (struct attr *);
extern struct attr *bgp_attr_default_set (struct attr *attr, u_char);
extern struct attr *bgp_attr_default_intern (u_char);
extern struct attr *bgp_attr_aggregate_intern (struct bgp *, u_char,
                                        struct aspath *, 
                                        struct community *, int as_set);
extern bgp_size_t bgp_packet_attribute (struct bgp *bgp, struct peer *,
					struct stream *, struct attr *,
					struct prefix *, afi_t, safi_t,
					struct peer *, struct prefix_rd *,
					u_char *);
extern void bgp_dump_routes_attr (struct stream *, struct attr *,
				  struct prefix *);
extern int attrhash_cmp (const void *, const void *);
extern unsigned int attrhash_key_make (void *);
extern void attr_show_all (struct vty *);
extern unsigned long int attr_count (void);
extern unsigned long int attr_unknown_count (void);

/* Cluster list prototypes. */
extern int cluster_loop_check (struct cluster_list *, struct in_addr);
extern void cluster_unintern (struct cluster_list *);

/* Transit attribute prototypes. */
void transit_unintern (struct transit *);

/* Below exported for unit-test purposes only */
struct bgp_attr_parser_args {
  struct peer *peer;
  bgp_size_t length; /* attribute data length; */
  bgp_size_t total; /* total length, inc header */
  struct attr *attr;
  u_int8_t type;
  u_int8_t flags;
  u_char *startp;   
};
extern int bgp_mp_reach_parse (struct bgp_attr_parser_args *args, 
			       struct bgp_nlri *);
extern int bgp_mp_unreach_parse (struct bgp_attr_parser_args *args,
                                 struct bgp_nlri *);

/**
 * Set of functions to encode MP_REACH_NLRI and MP_UNREACH_NLRI attributes.
 * Typical call sequence is to call _start(), followed by multiple _prefix(),
 * one for each NLRI that needs to be encoded into the UPDATE message, and
 * finally the _end() function.
 */
extern size_t bgp_packet_mpattr_start(struct stream *s, afi_t afi, safi_t safi,
				      struct attr *attr);
extern void bgp_packet_mpattr_prefix(struct stream *s, afi_t afi, safi_t safi,
				     struct prefix *p, struct prefix_rd *prd,
				     u_char *tag);
extern void bgp_packet_mpattr_end(struct stream *s, size_t sizep);

extern size_t bgp_packet_mpunreach_start (struct stream *s, afi_t afi,
					  safi_t safi);
extern void bgp_packet_mpunreach_prefix (struct stream *s, struct prefix *p,
			     afi_t afi, safi_t safi, struct prefix_rd *prd,
			     u_char *tag);
extern void bgp_packet_mpunreach_end (struct stream *s, size_t attrlen_pnt);

#endif /* _QUAGGA_BGP_ATTR_H */
