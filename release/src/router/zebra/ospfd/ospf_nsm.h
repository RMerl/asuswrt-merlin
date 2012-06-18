/*
 * OSPF version 2  Neighbor State Machine
 *   From RFC2328 [OSPF Version 2]
 *   Copyright (C) 1999 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _ZEBRA_OSPF_NSM_H
#define _ZEBRA_OSPF_NSM_H

/* OSPF Neighbor State Machine State. */
#define NSM_DependUpon          0
#define NSM_Down		1
#define NSM_Attempt		2
#define NSM_Init		3
#define NSM_TwoWay		4
#define NSM_ExStart		5
#define NSM_Exchange		6
#define NSM_Loading		7
#define NSM_Full		8
#define OSPF_NSM_STATE_MAX      9

/* OSPF Neighbor State Machine Event. */
#define NSM_NoEvent	        0
#define NSM_HelloReceived	1
#define NSM_Start		2
#define NSM_TwoWayReceived	3
#define NSM_NegotiationDone	4
#define NSM_ExchangeDone	5
#define NSM_BadLSReq		6
#define NSM_LoadingDone		7
#define NSM_AdjOK		8
#define NSM_SeqNumberMismatch	9
#define NSM_OneWayReceived     10
#define NSM_KillNbr	       11
#define NSM_InactivityTimer    12
#define NSM_LLDown	       13
#define OSPF_NSM_EVENT_MAX     14

/* Macro for OSPF NSM timer turn on. */
#define OSPF_NSM_TIMER_ON(T,F,V)                                              \
      do {                                                                    \
        if (!(T))                                                             \
          (T) = thread_add_timer (master, (F), nbr, (V));                     \
      } while (0)

/* Macro for OSPF NSM timer turn off. */
#define OSPF_NSM_TIMER_OFF(X)                                                 \
      do {                                                                    \
        if (X)                                                                \
          {                                                                   \
            thread_cancel (X);                                                \
            (X) = NULL;                                                       \
          }                                                                   \
      } while (0)

/* Macro for OSPF NSM schedule event. */
#define OSPF_NSM_EVENT_SCHEDULE(N,E)                                          \
      thread_add_event (master, ospf_nsm_event, (N), (E))

/* Macro for OSPF NSM execute event. */
#define OSPF_NSM_EVENT_EXECUTE(N,E)                                           \
      thread_execute (master, ospf_nsm_event, (N), (E))

/* Prototypes. */
int ospf_nsm_event (struct thread *);
void nsm_change_state (struct ospf_neighbor *, int);
void ospf_check_nbr_loading (struct ospf_neighbor *);
int ospf_db_summary_isempty (struct ospf_neighbor *);
int ospf_db_summary_count (struct ospf_neighbor *);
void ospf_db_summary_clear (struct ospf_neighbor *);

#endif /* _ZEBRA_OSPF_NSM_H */

