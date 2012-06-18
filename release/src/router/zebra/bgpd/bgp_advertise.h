/* BGP advertisement and adjacency
   Copyright (C) 1996, 97, 98, 99, 2000 Kunihiro Ishiguro

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

/* BGP advertise FIFO.  */
struct bgp_advertise_fifo
{
  struct bgp_advertise *next;
  struct bgp_advertise *prev;
};

/* BGP advertise attribute.  */
struct bgp_advertise_attr
{
  /* Head of advertisement pointer. */
  struct bgp_advertise *adv;

  /* Reference counter.  */
  unsigned long refcnt;

  /* Attribute pointer to be announced.  */
  struct attr *attr;
};

struct bgp_advertise
{
  /* FIFO for advertisement.  */
  struct bgp_advertise_fifo fifo;

  /* Link list for same attribute advertise.  */
  struct bgp_advertise *next;
  struct bgp_advertise *prev;

  /* Prefix information.  */
  struct bgp_node *rn;

  /* Reference pointer.  */
  struct bgp_adj_out *adj;

  /* Advertisement attribute.  */
  struct bgp_advertise_attr *baa;

  /* BGP info.  */
  struct bgp_info *binfo;
};

/* BGP adjacency out.  */
struct bgp_adj_out
{
  /* Lined list pointer.  */
  struct bgp_adj_out *next;
  struct bgp_adj_out *prev;

  /* Advertised peer.  */
  struct peer *peer;

  /* Advertised attribute.  */
  struct attr *attr;

  /* Advertisement information.  */
  struct bgp_advertise *adv;
};

/* BGP adjacency in. */
struct bgp_adj_in
{
  /* Linked list pointer.  */
  struct bgp_adj_in *next;
  struct bgp_adj_in *prev;

  /* Received peer.  */
  struct peer *peer;

  /* Received attribute.  */
  struct attr *attr;
};

/* BGP advertisement list.  */
struct bgp_synchronize
{
  struct bgp_advertise_fifo update;
  struct bgp_advertise_fifo withdraw;
  struct bgp_advertise_fifo withdraw_low;
};

/* BGP adjacency linked list.  */
#define BGP_INFO_ADD(N,A,TYPE)                        \
  do {                                                \
    (A)->prev = NULL;                                 \
    (A)->next = (N)->TYPE;                            \
    if ((N)->TYPE)                                    \
      (N)->TYPE->prev = (A);                          \
    (N)->TYPE = (A);                                  \
  } while (0)

#define BGP_INFO_DEL(N,A,TYPE)                        \
  do {                                                \
    if ((A)->next)                                    \
      (A)->next->prev = (A)->prev;                    \
    if ((A)->prev)                                    \
      (A)->prev->next = (A)->next;                    \
    else                                              \
      (N)->TYPE = (A)->next;                          \
  } while (0)

#define BGP_ADJ_IN_ADD(N,A)    BGP_INFO_ADD(N,A,adj_in)
#define BGP_ADJ_IN_DEL(N,A)    BGP_INFO_DEL(N,A,adj_in)
#define BGP_ADJ_OUT_ADD(N,A)   BGP_INFO_ADD(N,A,adj_out)
#define BGP_ADJ_OUT_DEL(N,A)   BGP_INFO_DEL(N,A,adj_out)

/* Prototypes.  */
void bgp_adj_out_set (struct bgp_node *, struct peer *, struct prefix *,
		      struct attr *, afi_t, safi_t, struct bgp_info *);
void bgp_adj_out_unset (struct bgp_node *, struct peer *, struct prefix *,
			afi_t, safi_t);
void bgp_adj_out_remove (struct bgp_node *, struct bgp_adj_out *, 
			 struct peer *, afi_t, safi_t);
int bgp_adj_out_lookup (struct peer *, struct prefix *, afi_t, safi_t,
			struct bgp_node *);

void bgp_adj_in_set (struct bgp_node *, struct peer *, struct attr *);
void bgp_adj_in_unset (struct bgp_node *, struct peer *);
void bgp_adj_in_remove (struct bgp_node *, struct bgp_adj_in *);

struct bgp_advertise *
bgp_advertise_clean (struct peer *, struct bgp_adj_out *, afi_t, safi_t);

void bgp_sync_init (struct peer *);
void bgp_sync_delete (struct peer *);
