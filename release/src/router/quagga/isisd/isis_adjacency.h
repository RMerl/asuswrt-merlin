/*
 * IS-IS Rout(e)ing protocol - isis_adjacency.h   
 *                             IS-IS adjacency handling
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
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

#ifndef _ZEBRA_ISIS_ADJACENCY_H
#define _ZEBRA_ISIS_ADJACENCY_H

enum isis_adj_usage
{
  ISIS_ADJ_NONE,
  ISIS_ADJ_LEVEL1,
  ISIS_ADJ_LEVEL2,
  ISIS_ADJ_LEVEL1AND2
};

enum isis_system_type
{
  ISIS_SYSTYPE_UNKNOWN,
  ISIS_SYSTYPE_ES,
  ISIS_SYSTYPE_IS,
  ISIS_SYSTYPE_L1_IS,
  ISIS_SYSTYPE_L2_IS
};

enum isis_adj_state
{
  ISIS_ADJ_UNKNOWN,
  ISIS_ADJ_INITIALIZING,
  ISIS_ADJ_UP,
  ISIS_ADJ_DOWN
};

/*
 * we use the following codes to give an indication _why_
 * a specific adjacency is up or down
 */
enum isis_adj_updown_reason
{
  ISIS_ADJ_REASON_SEENSELF,
  ISIS_ADJ_REASON_AREA_MISMATCH,
  ISIS_ADJ_REASON_HOLDTIMER_EXPIRED,
  ISIS_ADJ_REASON_AUTH_FAILED,
  ISIS_ADJ_REASON_CHECKSUM_FAILED
};

#define DIS_RECORDS 8	/* keep the last 8 DIS state changes on record */

struct isis_dis_record
{
  int dis;			/* is our neighbor the DIS ? */
  time_t last_dis_change;	/* timestamp for last dis change */
};

struct isis_adjacency
{
  u_char snpa[ETH_ALEN];		/* NeighbourSNPAAddress */
  u_char sysid[ISIS_SYS_ID_LEN];	/* neighbourSystemIdentifier */
  u_char lanid[ISIS_SYS_ID_LEN + 1];	/* LAN id on bcast circuits */
  int dischanges[ISIS_LEVELS];		/* how many DIS changes ? */
  /* an array of N levels for M records */
  struct isis_dis_record dis_record[DIS_RECORDS * ISIS_LEVELS];
  enum isis_adj_state adj_state;	/* adjacencyState */
  enum isis_adj_usage adj_usage;	/* adjacencyUsage */
  struct list *area_addrs;		/* areaAdressesOfNeighbour */
  struct nlpids nlpids;			/* protocols spoken ... */
  struct list *ipv4_addrs;
  struct in_addr router_address;
#ifdef HAVE_IPV6
  struct list *ipv6_addrs;
  struct in6_addr router_address6;
#endif				/* HAVE_IPV6 */
  u_char prio[ISIS_LEVELS];	/* priorityOfNeighbour for DIS */
  int circuit_t;		/* from hello PDU hdr */
  int level;			/* level (1 or 2) */
  enum isis_system_type sys_type;	/* neighbourSystemType */
  u_int16_t hold_time;		/* entryRemainingTime */
  u_int32_t last_upd;
  u_int32_t last_flap;		/* last time the adj flapped */
  int flaps;			/* number of adjacency flaps  */
  struct thread *t_expire;	/* expire after hold_time  */
  struct isis_circuit *circuit;	/* back pointer */
};

struct isis_adjacency *isis_adj_lookup (u_char * sysid, struct list *adjdb);
struct isis_adjacency *isis_adj_lookup_snpa (u_char * ssnpa,
					     struct list *adjdb);
struct isis_adjacency *isis_new_adj (u_char * id, u_char * snpa, int level,
				     struct isis_circuit *circuit);
void isis_delete_adj (void *adj);
void isis_adj_state_change (struct isis_adjacency *adj,
			    enum isis_adj_state state, const char *reason);
void isis_adj_print (struct isis_adjacency *adj);
int isis_adj_expire (struct thread *thread);
void isis_adj_print_vty (struct isis_adjacency *adj, struct vty *vty, char detail);
void isis_adj_build_neigh_list (struct list *adjdb, struct list *list);
void isis_adj_build_up_list (struct list *adjdb, struct list *list);

#endif /* ISIS_ADJACENCY_H */
