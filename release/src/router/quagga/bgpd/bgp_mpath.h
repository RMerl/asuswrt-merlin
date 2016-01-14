/* $QuaggaId: Format:%an, %ai, %h$ $
 *
 * BGP Multipath
 * Copyright (C) 2010 Google Inc.
 *
 * This file is part of Quagga
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

#ifndef _QUAGGA_BGP_MPATH_H
#define _QUAGGA_BGP_MPATH_H

/* BGP default maximum-paths */
#define BGP_DEFAULT_MAXPATHS 1

/* Supplemental information linked to bgp_info for keeping track of
 * multipath selections, lazily allocated to save memory
 */
struct bgp_info_mpath
{
  /* Points to the first multipath (on bestpath) or the next multipath */
  struct bgp_info_mpath *mp_next;

  /* Points to the previous multipath or NULL on bestpath */
  struct bgp_info_mpath *mp_prev;

  /* Points to bgp_info associated with this multipath info */
  struct bgp_info *mp_info;

  /* When attached to best path, the number of selected multipaths */
  u_int32_t mp_count;

  /* Aggregated attribute for advertising multipath route */
  struct attr *mp_attr;
};

/* Functions to support maximum-paths configuration */
extern int bgp_maximum_paths_set (struct bgp *, afi_t, safi_t, int, u_int16_t);
extern int bgp_maximum_paths_unset (struct bgp *, afi_t, safi_t, int);

/* Functions used by bgp_best_selection to record current
 * multipath selections
 */
extern void bgp_mp_list_init (struct list *);
extern void bgp_mp_list_clear (struct list *);
extern void bgp_mp_list_add (struct list *, struct bgp_info *);
extern void bgp_mp_dmed_deselect (struct bgp_info *);
extern void bgp_info_mpath_update (struct bgp_node *, struct bgp_info *,
                                   struct bgp_info *, struct list *,
                                   struct bgp_maxpaths_cfg *);
extern void bgp_info_mpath_aggregate_update (struct bgp_info *,
                                             struct bgp_info *);

/* Unlink and free multipath information associated with a bgp_info */
extern void bgp_info_mpath_dequeue (struct bgp_info *);
extern void bgp_info_mpath_free (struct bgp_info_mpath **);

/* Walk list of multipaths associated with a best path */
extern struct bgp_info *bgp_info_mpath_first (struct bgp_info *);
extern struct bgp_info *bgp_info_mpath_next (struct bgp_info *);

/* Accessors for multipath information */
extern u_int32_t bgp_info_mpath_count (struct bgp_info *);
extern struct attr *bgp_info_mpath_attr (struct bgp_info *);

#endif /* _QUAGGA_BGP_MPATH_H */
