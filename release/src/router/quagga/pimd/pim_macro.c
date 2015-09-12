/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"

#include "pim_macro.h"
#include "pimd.h"
#include "pim_str.h"
#include "pim_iface.h"
#include "pim_ifchannel.h"

#define PIM_IFP_I_am_DR(pim_ifp) ((pim_ifp)->pim_dr_addr.s_addr == (pim_ifp)->primary_address.s_addr)

/*
  DownstreamJPState(S,G,I) is the per-interface state machine for
  receiving (S,G) Join/Prune messages.

  DownstreamJPState(S,G,I) is either Join or Prune-Pending ?
*/
static int downstream_jpstate_isjoined(const struct pim_ifchannel *ch)
{
  return ch->ifjoin_state != PIM_IFJOIN_NOINFO;
}

/*
  The clause "local_receiver_include(S,G,I)" is true if the IGMP/MLD
  module or other local membership mechanism has determined that local
  members on interface I desire to receive traffic sent specifically
  by S to G.
*/
static int local_receiver_include(const struct pim_ifchannel *ch)
{
  /* local_receiver_include(S,G,I) ? */
  return ch->local_ifmembership == PIM_IFMEMBERSHIP_INCLUDE;
}

/*
  RFC 4601: 4.1.6.  State Summarization Macros

   The set "joins(S,G)" is the set of all interfaces on which the
   router has received (S,G) Joins:

   joins(S,G) =
       { all interfaces I such that
         DownstreamJPState(S,G,I) is either Join or Prune-Pending }

  DownstreamJPState(S,G,I) is either Join or Prune-Pending ?
*/
int pim_macro_chisin_joins(const struct pim_ifchannel *ch)
{
  return downstream_jpstate_isjoined(ch);
}

/*
  RFC 4601: 4.6.5.  Assert State Macros

   The set "lost_assert(S,G)" is the set of all interfaces on which the
   router has received (S,G) joins but has lost an (S,G) assert.

   lost_assert(S,G) =
       { all interfaces I such that
         lost_assert(S,G,I) == TRUE }

     bool lost_assert(S,G,I) {
       if ( RPF_interface(S) == I ) {
          return FALSE
       } else {
          return ( AssertWinner(S,G,I) != NULL AND
                   AssertWinner(S,G,I) != me  AND
                   (AssertWinnerMetric(S,G,I) is better
                      than spt_assert_metric(S,I) )
       }
     }

  AssertWinner(S,G,I) is the IP source address of the Assert(S,G)
  packet that won an Assert.
*/
int pim_macro_ch_lost_assert(const struct pim_ifchannel *ch)
{
  struct interface *ifp;
  struct pim_interface *pim_ifp;
  struct pim_assert_metric spt_assert_metric;

  ifp = ch->interface;
  if (!ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): null interface",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str);
    return 0; /* false */
  }

  /* RPF_interface(S) == I ? */
  if (ch->upstream->rpf.source_nexthop.interface == ifp)
    return 0; /* false */

  pim_ifp = ifp->info;
  if (!pim_ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): multicast not enabled on interface %s",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str, ifp->name);
    return 0; /* false */
  }

  if (PIM_INADDR_IS_ANY(ch->ifassert_winner))
    return 0; /* false */

  /* AssertWinner(S,G,I) == me ? */
  if (ch->ifassert_winner.s_addr == pim_ifp->primary_address.s_addr)
    return 0; /* false */

  spt_assert_metric = pim_macro_spt_assert_metric(&ch->upstream->rpf,
						  pim_ifp->primary_address);

  return pim_assert_metric_better(&ch->ifassert_winner_metric,
				  &spt_assert_metric);
}

/*
  RFC 4601: 4.1.6.  State Summarization Macros

   pim_include(S,G) =
       { all interfaces I such that:
         ( (I_am_DR( I ) AND lost_assert(S,G,I) == FALSE )
           OR AssertWinner(S,G,I) == me )
          AND  local_receiver_include(S,G,I) }

   AssertWinner(S,G,I) is the IP source address of the Assert(S,G)
   packet that won an Assert.
*/
int pim_macro_chisin_pim_include(const struct pim_ifchannel *ch)
{
  struct pim_interface *pim_ifp = ch->interface->info;

  if (!pim_ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): multicast not enabled on interface %s",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str, ch->interface->name);
    return 0; /* false */
  }

  /* local_receiver_include(S,G,I) ? */
  if (!local_receiver_include(ch))
    return 0; /* false */
    
  /* OR AssertWinner(S,G,I) == me ? */
  if (ch->ifassert_winner.s_addr == pim_ifp->primary_address.s_addr)
    return 1; /* true */
    
  return (
	  /* I_am_DR( I ) ? */
	  PIM_IFP_I_am_DR(pim_ifp)
	  &&
	  /* lost_assert(S,G,I) == FALSE ? */
	  (!pim_macro_ch_lost_assert(ch))
	  );
}

int pim_macro_chisin_joins_or_include(const struct pim_ifchannel *ch)
{
  if (pim_macro_chisin_joins(ch))
    return 1; /* true */

  return pim_macro_chisin_pim_include(ch);
}

/*
  RFC 4601: 4.6.1.  (S,G) Assert Message State Machine

  CouldAssert(S,G,I) =
  SPTbit(S,G)==TRUE
  AND (RPF_interface(S) != I)
  AND (I in ( ( joins(*,*,RP(G)) (+) joins(*,G) (-) prunes(S,G,rpt) )
                 (+) ( pim_include(*,G) (-) pim_exclude(S,G) )
                 (-) lost_assert(*,G)
                 (+) joins(S,G) (+) pim_include(S,G) ) )

  CouldAssert(S,G,I) is true for downstream interfaces that would be in
  the inherited_olist(S,G) if (S,G) assert information was not taken
  into account.

  CouldAssert(S,G,I) may be affected by changes in the following:

  pim_ifp->primary_address
  pim_ifp->pim_dr_addr
  ch->ifassert_winner_metric
  ch->ifassert_winner
  ch->local_ifmembership
  ch->ifjoin_state
  ch->upstream->rpf.source_nexthop.mrib_metric_preference
  ch->upstream->rpf.source_nexthop.mrib_route_metric
  ch->upstream->rpf.source_nexthop.interface
*/
int pim_macro_ch_could_assert_eval(const struct pim_ifchannel *ch)
{
  struct interface *ifp;

  /* SPTbit(S,G) is always true for PIM-SSM-Only Routers */

  ifp = ch->interface;
  if (!ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): null interface",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str);
    return 0; /* false */
  }

  /* RPF_interface(S) != I ? */
  if (ch->upstream->rpf.source_nexthop.interface == ifp)
    return 0; /* false */

  /* I in joins(S,G) (+) pim_include(S,G) ? */
  return pim_macro_chisin_joins_or_include(ch);
}

/*
  RFC 4601: 4.6.3.  Assert Metrics

   spt_assert_metric(S,I) gives the assert metric we use if we're
   sending an assert based on active (S,G) forwarding state:

    assert_metric
    spt_assert_metric(S,I) {
      return {0,MRIB.pref(S),MRIB.metric(S),my_ip_address(I)}
    }
*/
struct pim_assert_metric pim_macro_spt_assert_metric(const struct pim_rpf *rpf,
						     struct in_addr ifaddr)
{
  struct pim_assert_metric metric;

  metric.rpt_bit_flag      = 0;
  metric.metric_preference = rpf->source_nexthop.mrib_metric_preference;
  metric.route_metric      = rpf->source_nexthop.mrib_route_metric;
  metric.ip_address        = ifaddr;

  return metric;
}

/*
  RFC 4601: 4.6.3.  Assert Metrics

   An assert metric for (S,G) to include in (or compare against) an
   Assert message sent on interface I should be computed using the
   following pseudocode:

  assert_metric  my_assert_metric(S,G,I) {
    if( CouldAssert(S,G,I) == TRUE ) {
      return spt_assert_metric(S,I)
    } else if( CouldAssert(*,G,I) == TRUE ) {
      return rpt_assert_metric(G,I)
    } else {
      return infinite_assert_metric()
    }
  }
*/
struct pim_assert_metric pim_macro_ch_my_assert_metric_eval(const struct pim_ifchannel *ch)
{
  struct pim_interface *pim_ifp;

  pim_ifp = ch->interface->info;

  if (pim_ifp) {
    if (PIM_IF_FLAG_TEST_COULD_ASSERT(ch->flags)) {
      return pim_macro_spt_assert_metric(&ch->upstream->rpf, pim_ifp->primary_address);
    }
  }

  return qpim_infinite_assert_metric;
}

/*
  RFC 4601 4.2.  Data Packet Forwarding Rules
  RFC 4601 4.8.2.  PIM-SSM-Only Routers
  
  Macro:
  inherited_olist(S,G) =
    joins(S,G) (+) pim_include(S,G) (-) lost_assert(S,G)
*/
static int pim_macro_chisin_inherited_olist(const struct pim_ifchannel *ch)
{
  if (pim_macro_ch_lost_assert(ch))
    return 0; /* false */

  return pim_macro_chisin_joins_or_include(ch);
}

/*
  RFC 4601 4.2.  Data Packet Forwarding Rules
  RFC 4601 4.8.2.  PIM-SSM-Only Routers

  Additionally, the Packet forwarding rules of Section 4.2 can be
  simplified in a PIM-SSM-only router:
  
  iif is the incoming interface of the packet.
  oiflist = NULL
  if (iif == RPF_interface(S) AND UpstreamJPState(S,G) == Joined) {
    oiflist = inherited_olist(S,G)
  } else if (iif is in inherited_olist(S,G)) {
    send Assert(S,G) on iif
  }
  oiflist = oiflist (-) iif
  forward packet on all interfaces in oiflist
  
  Macro:
  inherited_olist(S,G) =
    joins(S,G) (+) pim_include(S,G) (-) lost_assert(S,G)

  Note:
  - The following test is performed as response to WRONGVIF kernel
    upcall:
    if (iif is in inherited_olist(S,G)) {
      send Assert(S,G) on iif
    }
    See pim_mroute.c mroute_msg().
*/
int pim_macro_chisin_oiflist(const struct pim_ifchannel *ch)
{
  if (ch->upstream->join_state != PIM_UPSTREAM_JOINED) {
    /* oiflist is NULL */
    return 0; /* false */
  }

  /* oiflist = oiflist (-) iif */
  if (ch->interface == ch->upstream->rpf.source_nexthop.interface)
    return 0; /* false */

  return pim_macro_chisin_inherited_olist(ch);
}

/*
  RFC 4601: 4.6.1.  (S,G) Assert Message State Machine

  AssertTrackingDesired(S,G,I) =
  (I in ( ( joins(*,*,RP(G)) (+) joins(*,G) (-) prunes(S,G,rpt) )
	(+) ( pim_include(*,G) (-) pim_exclude(S,G) )
	(-) lost_assert(*,G)
	(+) joins(S,G) ) )
     OR (local_receiver_include(S,G,I) == TRUE
	 AND (I_am_DR(I) OR (AssertWinner(S,G,I) == me)))
     OR ((RPF_interface(S) == I) AND (JoinDesired(S,G) == TRUE))
     OR ((RPF_interface(RP(G)) == I) AND (JoinDesired(*,G) == TRUE)
	 AND (SPTbit(S,G) == FALSE))

  AssertTrackingDesired(S,G,I) is true on any interface in which an
  (S,G) assert might affect our behavior.
*/
int pim_macro_assert_tracking_desired_eval(const struct pim_ifchannel *ch)
{
  struct pim_interface *pim_ifp;
  struct interface *ifp;

  ifp = ch->interface;
  if (!ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): null interface",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str);
    return 0; /* false */
  }

  pim_ifp = ifp->info;
  if (!pim_ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): multicast not enabled on interface %s",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str, ch->interface->name);
    return 0; /* false */
  }

  /* I in joins(S,G) ? */
  if (pim_macro_chisin_joins(ch))
    return 1; /* true */

  /* local_receiver_include(S,G,I) ? */
  if (local_receiver_include(ch)) {
    /* I_am_DR(I) ? */
    if (PIM_IFP_I_am_DR(pim_ifp))
      return 1; /* true */

    /* AssertWinner(S,G,I) == me ? */
    if (ch->ifassert_winner.s_addr == pim_ifp->primary_address.s_addr)
      return 1; /* true */
  }

  /* RPF_interface(S) == I ? */
  if (ch->upstream->rpf.source_nexthop.interface == ifp) {
    /* JoinDesired(S,G) ? */
    if (PIM_UPSTREAM_FLAG_TEST_DR_JOIN_DESIRED(ch->upstream->flags))
      return 1; /* true */
  }

  return 0; /* false */
}

