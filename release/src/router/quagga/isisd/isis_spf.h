/*
 * IS-IS Rout(e)ing protocol - isis_spf.h
 *                             IS-IS Shortest Path First algorithm  
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _ZEBRA_ISIS_SPF_H
#define _ZEBRA_ISIS_SPF_H

enum vertextype
{
  VTYPE_PSEUDO_IS = 1,
  VTYPE_PSEUDO_TE_IS,
  VTYPE_NONPSEUDO_IS,
  VTYPE_NONPSEUDO_TE_IS,
  VTYPE_ES,
  VTYPE_IPREACH_INTERNAL,
  VTYPE_IPREACH_EXTERNAL,
  VTYPE_IPREACH_TE
#ifdef HAVE_IPV6
    ,
  VTYPE_IP6REACH_INTERNAL,
  VTYPE_IP6REACH_EXTERNAL
#endif /* HAVE_IPV6 */
};

/*
 * Triple <N, d(N), {Adj(N)}> 
 */
struct isis_vertex
{
  enum vertextype type;

  union
  {
    u_char id[ISIS_SYS_ID_LEN + 1];
    struct prefix prefix;
  } N;

  u_int32_t d_N;		/* d(N) Distance from this IS      */
  u_int16_t depth;		/* The depth in the imaginary tree */
  struct list *Adj_N;		/* {Adj(N)} next hop or neighbor list */
  struct list *parents;         /* list of parents for ECMP */
  struct list *children;        /* list of children used for tree dump */
};

struct isis_spftree
{
  struct thread *t_spf;		/* spf threads */
  struct list *paths;		/* the SPT */
  struct list *tents;		/* TENT */
  struct isis_area *area;       /* back pointer to area */
  int pending;			/* already scheduled */
  unsigned int runcount;        /* number of runs since uptime */
  time_t last_run_timestamp;    /* last run timestamp for scheduling */
  time_t last_run_duration;     /* last run duration in msec */
};

struct isis_spftree * isis_spftree_new (struct isis_area *area);
void isis_spftree_del (struct isis_spftree *spftree);
void isis_spftree_adj_del (struct isis_spftree *spftree,
                           struct isis_adjacency *adj);
void spftree_area_init (struct isis_area *area);
void spftree_area_del (struct isis_area *area);
void spftree_area_adj_del (struct isis_area *area,
                           struct isis_adjacency *adj);
int isis_spf_schedule (struct isis_area *area, int level);
void isis_spf_cmds_init (void);
#ifdef HAVE_IPV6
int isis_spf_schedule6 (struct isis_area *area, int level);
#endif
#endif /* _ZEBRA_ISIS_SPF_H */
