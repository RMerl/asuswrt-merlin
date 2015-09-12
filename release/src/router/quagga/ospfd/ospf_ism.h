/*
 * OSPF version 2  Interface State Machine.
 *   From RFC2328 [OSPF Version 2]
 * Copyright (C) 1999 Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_ISM_H
#define _ZEBRA_OSPF_ISM_H

/* OSPF Interface State Machine Status. */
#define ISM_DependUpon                    0
#define ISM_Down                          1
#define ISM_Loopback                      2
#define ISM_Waiting                       3
#define ISM_PointToPoint                  4
#define ISM_DROther                       5
#define ISM_Backup                        6
#define ISM_DR                            7
#define OSPF_ISM_STATE_MAX   	          8

/* Because DR/DROther values are exhanged wrt RFC */
#define ISM_SNMP(x) (((x) == ISM_DROther) ? ISM_DR : \
                     ((x) == ISM_DR) ? ISM_DROther : (x))

/* OSPF Interface State Machine Event. */
#define ISM_NoEvent                       0
#define ISM_InterfaceUp                   1
#define ISM_WaitTimer                     2
#define ISM_BackupSeen                    3
#define ISM_NeighborChange                4
#define ISM_LoopInd                       5
#define ISM_UnloopInd                     6
#define ISM_InterfaceDown                 7
#define OSPF_ISM_EVENT_MAX                8

#define OSPF_ISM_WRITE_ON(O)                                                  \
      do                                                                      \
        {                                                                     \
          if (oi->on_write_q == 0)                                            \
	    {                                                                 \
              listnode_add ((O)->oi_write_q, oi);                             \
	      oi->on_write_q = 1;                                             \
	    }                                                                 \
	  if ((O)->t_write == NULL)                                           \
	    (O)->t_write =                                                    \
	      thread_add_write (master, ospf_write, (O), (O)->fd);            \
        } while (0)
     
/* Macro for OSPF ISM timer turn on. */
#define OSPF_ISM_TIMER_ON(T,F,V) \
  do { \
    if (!(T)) \
      (T) = thread_add_timer (master, (F), oi, (V)); \
  } while (0)
#define OSPF_ISM_TIMER_MSEC_ON(T,F,V) \
  do { \
    if (!(T)) \
      (T) = thread_add_timer_msec (master, (F), oi, (V)); \
  } while (0)

/* convenience macro to set hello timer correctly, according to
 * whether fast-hello is set or not
 */
#define OSPF_HELLO_TIMER_ON(O) \
  do { \
    if (OSPF_IF_PARAM ((O), fast_hello)) \
        OSPF_ISM_TIMER_MSEC_ON ((O)->t_hello, ospf_hello_timer, \
                                1000 / OSPF_IF_PARAM ((O), fast_hello)); \
    else \
        OSPF_ISM_TIMER_ON ((O)->t_hello, ospf_hello_timer, \
                                OSPF_IF_PARAM ((O), v_hello)); \
  } while (0)

/* Macro for OSPF ISM timer turn off. */
#define OSPF_ISM_TIMER_OFF(X) \
  do { \
    if (X) \
      { \
	thread_cancel (X); \
	(X) = NULL; \
      } \
  } while (0)

/* Macro for OSPF schedule event. */
#define OSPF_ISM_EVENT_SCHEDULE(I,E) \
      thread_add_event (master, ospf_ism_event, (I), (E))

/* Macro for OSPF execute event. */
#define OSPF_ISM_EVENT_EXECUTE(I,E) \
      thread_execute (master, ospf_ism_event, (I), (E))

/* Prototypes. */
extern int ospf_ism_event (struct thread *);
extern void ism_change_status (struct ospf_interface *, int);
extern int ospf_hello_timer (struct thread *thread);

#endif /* _ZEBRA_OSPF_ISM_H */
