/* BGP flap dampening
   Copyright (C) 2001 IP Infusion Inc.

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

/* Structure maintained on a per-route basis. */
struct bgp_damp_info
{
  /* Doubly linked list.  This information must be linked to
     reuse_list or no_reuse_list.  */
  struct bgp_damp_info *next;
  struct bgp_damp_info *prev;

  /* Figure-of-merit.  */
  int penalty;

  /* Number of flapping.  */
  int flap;
	
  /* First flap time  */
  time_t start_time;
 
  /* Last time penalty was updated.  */
  time_t t_updated;

  /* Time of route start to be suppressed.  */
  time_t suppress_time;

  /* Back reference to bgp_info. */
  struct bgp_info *binfo;

  /* Back reference to bgp_node. */
  struct bgp_node *rn;

  /* Current index in the reuse_list. */
  int index;

  /* Last time message type. */
  u_char lastrecord;
#define BGP_RECORD_UPDATE	1
#define BGP_RECORD_WITHDRAW	2

  afi_t afi;
  safi_t safi;
};

/* Specified parameter set configuration. */
struct bgp_damp_config
{
  /* Value over which routes suppressed.  */
  int suppress_value;

  /* Value below which suppressed routes reused.  */
  int reuse_limit;    

  /* Max time a route can be suppressed.  */
  int max_suppress_time;      

  /* Time during which accumulated penalty reduces by half.  */
  int half_life; 

  /* Non-configurable parameters but fixed at implementation time.
   * To change this values, init_bgp_damp() should be modified.
   */
  int tmax;		  /* Max time previous instability retained */
  int reuse_list_size;		/* Number of reuse lists */
  int reuse_index_size;		/* Size of reuse index array */

  /* Non-configurable parameters.  Most of these are calculated from
   * the configurable parameters above.
   */
  unsigned int ceiling;		/* Max value a penalty can attain */
  int decay_rate_per_tick;	/* Calculated from half-life */
  int decay_array_size;		/* Calculated using config parameters */
  double scale_factor;
  int reuse_scale_factor; 
         
  /* Decay array per-set based. */ 
  double *decay_array;	

  /* Reuse index array per-set based. */ 
  int *reuse_index;

  /* Reuse list array per-set based. */  
  struct bgp_damp_info **reuse_list;
  int reuse_offset;
        
  /* All dampening information which is not on reuse list.  */
  struct bgp_damp_info *no_reuse_list;

  /* Reuse timer thread per-set base. */
  struct thread* t_reuse;
};

#define BGP_DAMP_NONE           0
#define BGP_DAMP_USED		1
#define BGP_DAMP_SUPPRESSED	2

/* Time granularity for reuse lists */
#define DELTA_REUSE	          10

/* Time granularity for decay arrays */
#define DELTA_T 	           5

#define DEFAULT_PENALTY         1000

#define DEFAULT_HALF_LIFE         15
#define DEFAULT_REUSE 	       	 750
#define DEFAULT_SUPPRESS 	2000

#define REUSE_LIST_SIZE          256
#define REUSE_ARRAY_SIZE        1024

int bgp_damp_enable (struct bgp *, afi_t, safi_t, int, int, int, int);
int bgp_damp_disable (struct bgp *, afi_t, safi_t);
int bgp_damp_withdraw (struct bgp_info *, struct bgp_node *,
		       afi_t, safi_t, int);
int bgp_damp_update (struct bgp_info *, struct bgp_node *, afi_t, safi_t);
int bgp_damp_scan (struct bgp_info *, afi_t, safi_t);
void bgp_damp_info_free (struct bgp_damp_info *, int);
void bgp_damp_info_clean ();
char * bgp_get_reuse_time (int, char*, size_t);
int bgp_damp_decay (time_t, int);
int bgp_config_write_damp (struct vty *);
void bgp_damp_info_vty (struct vty *, struct bgp_info *);
char * bgp_damp_reuse_time_vty (struct vty *, struct bgp_info *);
