/*
 * Copyright (C) 2003 Yasuhiro Ohara
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#ifndef OSPF6D_H
#define OSPF6D_H

#define OSPF6_DAEMON_VERSION    "0.9.7o"

/* global variables */
extern int errno;
extern struct thread_master *master;

#ifdef INRIA_IPV6
#ifndef IPV6_PKTINFO
#define IPV6_PKTINFO IPV6_RECVPKTINFO
#endif /* IPV6_PKTINFO */
#endif /* INRIA_IPV6 */

/* Historical for KAME.  */
#ifndef IPV6_JOIN_GROUP
#ifdef IPV6_ADD_MEMBERSHIP
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP
#endif /* IPV6_ADD_MEMBERSHIP. */
#ifdef IPV6_JOIN_MEMBERSHIP
#define IPV6_JOIN_GROUP  IPV6_JOIN_MEMBERSHIP
#endif /* IPV6_JOIN_MEMBERSHIP. */
#endif /* ! IPV6_JOIN_GROUP*/

#ifndef IPV6_LEAVE_GROUP
#ifdef IPV6_DROP_MEMBERSHIP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP
#endif /* IPV6_DROP_MEMBERSHIP */
#endif /* ! IPV6_LEAVE_GROUP */

/* cast macro */
#define OSPF6_PROCESS(x) ((struct ospf6 *) (x))
#define OSPF6_AREA(x) ((struct ospf6_area *) (x))
#define OSPF6_INTERFACE(x) ((struct ospf6_interface *) (x))
#define OSPF6_NEIGHBOR(x) ((struct ospf6_neighbor *) (x))

/* operation on timeval structure */
#ifndef timerclear
#define timerclear(a) (a)->tv_sec = (tvp)->tv_usec = 0
#endif /*timerclear*/
#ifndef timersub
#define timersub(a, b, res)                           \
  do {                                                \
    (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;        \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec;     \
    if ((res)->tv_usec < 0)                           \
      {                                               \
        (res)->tv_sec--;                              \
        (res)->tv_usec += 1000000;                    \
      }                                               \
  } while (0)
#endif /*timersub*/
#define timerstring(tv, buf, size)                    \
  do {                                                \
    if ((tv)->tv_sec / 60 / 60 / 24)                  \
      snprintf (buf, size, "%ldd%02ld:%02ld:%02ld",   \
                (tv)->tv_sec / 60 / 60 / 24,          \
                (tv)->tv_sec / 60 / 60 % 24,          \
                (tv)->tv_sec / 60 % 60,               \
                (tv)->tv_sec % 60);                   \
    else                                              \
      snprintf (buf, size, "%02ld:%02ld:%02ld",       \
                (tv)->tv_sec / 60 / 60 % 24,          \
                (tv)->tv_sec / 60 % 60,               \
                (tv)->tv_sec % 60);                   \
  } while (0)
#define timerstring_local(tv, buf, size)                  \
  do {                                                    \
    int ret;                                              \
    struct tm *tm;                                        \
    tm = localtime (&(tv)->tv_sec);                       \
    ret = strftime (buf, size, "%Y/%m/%d %H:%M:%S", tm);  \
    if (ret == 0)                                         \
      zlog_warn ("strftime error");                       \
  } while (0)

/* for commands */
#define OSPF6_AREA_STR      "Area information\n"
#define OSPF6_AREA_ID_STR   "Area ID (as an IPv4 notation)\n"
#define OSPF6_SPF_STR       "Shortest Path First tree information\n"
#define OSPF6_ROUTER_ID_STR "Specify Router-ID\n"
#define OSPF6_LS_ID_STR     "Specify Link State ID\n"

#define VNL VTY_NEWLINE
#define OSPF6_CMD_CHECK_RUNNING() \
  if (ospf6 == NULL) \
    { \
      vty_out (vty, "OSPFv3 is not running%s", VTY_NEWLINE); \
      return CMD_SUCCESS; \
    }


/* Function Prototypes */
struct route_node *route_prev (struct route_node *node);

void ospf6_debug ();
void ospf6_init ();

#endif /* OSPF6D_H */


