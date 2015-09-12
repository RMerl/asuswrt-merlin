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

#ifndef PIMD_H
#define PIMD_H

#include <stdint.h>

#include "pim_mroute.h"
#include "pim_assert.h"

#define PIMD_PROGNAME       "pimd"
#define PIMD_DEFAULT_CONFIG "pimd.conf"
#define PIMD_VTY_PORT       2611
#define PIMD_BUG_ADDRESS    "https://github.com/udhos/qpimd"

#define PIM_IP_HEADER_MIN_LEN         (20)
#define PIM_IP_HEADER_MAX_LEN         (60)
#define PIM_IP_PROTO_IGMP             (2)
#define PIM_IP_PROTO_PIM              (103)
#define PIM_IGMP_MIN_LEN              (8)
#define PIM_MSG_HEADER_LEN            (4)
#define PIM_PIM_MIN_LEN               PIM_MSG_HEADER_LEN
#define PIM_PROTO_VERSION             (2)

#define MCAST_ALL_SYSTEMS      "224.0.0.1"
#define MCAST_ALL_ROUTERS      "224.0.0.2"
#define MCAST_ALL_PIM_ROUTERS  "224.0.0.13"
#define MCAST_ALL_IGMP_ROUTERS "224.0.0.22"

#define PIM_FORCE_BOOLEAN(expr) ((expr) != 0)

#define PIM_NET_INADDR_ANY (htonl(INADDR_ANY))
#define PIM_INADDR_IS_ANY(addr) ((addr).s_addr == PIM_NET_INADDR_ANY) /* struct in_addr addr */
#define PIM_INADDR_ISNOT_ANY(addr) ((addr).s_addr != PIM_NET_INADDR_ANY) /* struct in_addr addr */

#define PIM_MASK_PIM_EVENTS          (1 << 0)
#define PIM_MASK_PIM_PACKETS         (1 << 1)
#define PIM_MASK_PIM_PACKETDUMP_SEND (1 << 2)
#define PIM_MASK_PIM_PACKETDUMP_RECV (1 << 3)
#define PIM_MASK_PIM_TRACE           (1 << 4)
#define PIM_MASK_IGMP_EVENTS         (1 << 5)
#define PIM_MASK_IGMP_PACKETS        (1 << 6)
#define PIM_MASK_IGMP_TRACE          (1 << 7)
#define PIM_MASK_ZEBRA               (1 << 8)
#define PIM_MASK_SSMPINGD            (1 << 9)
#define PIM_MASK_MROUTE              (1 << 10)
#define PIM_MASK_PIM_HELLO	     (1 << 11)
#define PIM_MASK_PIM_J_P	     (1 << 12)

const char *const PIM_ALL_SYSTEMS;
const char *const PIM_ALL_ROUTERS;
const char *const PIM_ALL_PIM_ROUTERS;
const char *const PIM_ALL_IGMP_ROUTERS;

struct thread_master     *master;
uint32_t                  qpim_debugs;
int                       qpim_mroute_socket_fd;
int64_t                   qpim_mroute_socket_creation; /* timestamp of creation */
struct thread            *qpim_mroute_socket_reader;
int                       qpim_mroute_oif_highest_vif_index;
struct list              *qpim_channel_oil_list; /* list of struct channel_oil */
struct in_addr            qpim_all_pim_routers_addr;
int                       qpim_t_periodic; /* Period between Join/Prune Messages */
struct list              *qpim_upstream_list; /* list of struct pim_upstream */
struct zclient           *qpim_zclient_update;
struct zclient           *qpim_zclient_lookup;
struct pim_assert_metric  qpim_infinite_assert_metric;
long                      qpim_rpf_cache_refresh_delay_msec;
struct thread            *qpim_rpf_cache_refresher;
int64_t                   qpim_rpf_cache_refresh_requests;
int64_t                   qpim_rpf_cache_refresh_events;
int64_t                   qpim_rpf_cache_refresh_last;
struct in_addr            qpim_inaddr_any;
struct list              *qpim_ssmpingd_list; /* list of struct ssmpingd_sock */
struct in_addr            qpim_ssmpingd_group_addr;
int64_t                   qpim_scan_oil_events;
int64_t                   qpim_scan_oil_last;
int64_t                   qpim_mroute_add_events;
int64_t                   qpim_mroute_add_last;
int64_t                   qpim_mroute_del_events;
int64_t                   qpim_mroute_del_last;

#define PIM_JP_HOLDTIME (qpim_t_periodic * 7 / 2)

#define PIM_MROUTE_IS_ENABLED  (qpim_mroute_socket_fd >= 0)
#define PIM_MROUTE_IS_DISABLED (qpim_mroute_socket_fd < 0)

#define PIM_DEBUG_PIM_EVENTS          (qpim_debugs & PIM_MASK_PIM_EVENTS)
#define PIM_DEBUG_PIM_PACKETS         (qpim_debugs & PIM_MASK_PIM_PACKETS)
#define PIM_DEBUG_PIM_PACKETDUMP_SEND (qpim_debugs & PIM_MASK_PIM_PACKETDUMP_SEND)
#define PIM_DEBUG_PIM_PACKETDUMP_RECV (qpim_debugs & PIM_MASK_PIM_PACKETDUMP_RECV)
#define PIM_DEBUG_PIM_TRACE           (qpim_debugs & PIM_MASK_PIM_TRACE)
#define PIM_DEBUG_IGMP_EVENTS         (qpim_debugs & PIM_MASK_IGMP_EVENTS)
#define PIM_DEBUG_IGMP_PACKETS        (qpim_debugs & PIM_MASK_IGMP_PACKETS)
#define PIM_DEBUG_IGMP_TRACE          (qpim_debugs & PIM_MASK_IGMP_TRACE)
#define PIM_DEBUG_ZEBRA               (qpim_debugs & PIM_MASK_ZEBRA)
#define PIM_DEBUG_SSMPINGD            (qpim_debugs & PIM_MASK_SSMPINGD)
#define PIM_DEBUG_MROUTE              (qpim_debugs & PIM_MASK_MROUTE)
#define PIM_DEBUG_PIM_HELLO	      (qpim_debugs & PIM_MASK_PIM_HELLO)
#define PIM_DEBUG_PIM_J_P	      (qpim_debugs & PIM_MASK_PIM_J_P)

#define PIM_DEBUG_EVENTS       (qpim_debugs & (PIM_MASK_PIM_EVENTS | PIM_MASK_IGMP_EVENTS))
#define PIM_DEBUG_PACKETS      (qpim_debugs & (PIM_MASK_PIM_PACKETS | PIM_MASK_IGMP_PACKETS))
#define PIM_DEBUG_TRACE        (qpim_debugs & (PIM_MASK_PIM_TRACE | PIM_MASK_IGMP_TRACE))

#define PIM_DO_DEBUG_PIM_EVENTS          (qpim_debugs |= PIM_MASK_PIM_EVENTS)
#define PIM_DO_DEBUG_PIM_PACKETS         (qpim_debugs |= PIM_MASK_PIM_PACKETS)
#define PIM_DO_DEBUG_PIM_PACKETDUMP_SEND (qpim_debugs |= PIM_MASK_PIM_PACKETDUMP_SEND)
#define PIM_DO_DEBUG_PIM_PACKETDUMP_RECV (qpim_debugs |= PIM_MASK_PIM_PACKETDUMP_RECV)
#define PIM_DO_DEBUG_PIM_TRACE           (qpim_debugs |= PIM_MASK_PIM_TRACE)
#define PIM_DO_DEBUG_IGMP_EVENTS         (qpim_debugs |= PIM_MASK_IGMP_EVENTS)
#define PIM_DO_DEBUG_IGMP_PACKETS        (qpim_debugs |= PIM_MASK_IGMP_PACKETS)
#define PIM_DO_DEBUG_IGMP_TRACE          (qpim_debugs |= PIM_MASK_IGMP_TRACE)
#define PIM_DO_DEBUG_ZEBRA               (qpim_debugs |= PIM_MASK_ZEBRA)
#define PIM_DO_DEBUG_SSMPINGD            (qpim_debugs |= PIM_MASK_SSMPINGD)
#define PIM_DO_DEBUG_MROUTE              (qpim_debugs |= PIM_MASK_MROUTE)
#define PIM_DO_DEBUG_PIM_HELLO           (qpim_debugs |= PIM_MASK_PIM_HELLO)
#define PIM_DO_DEBUG_PIM_J_P             (qpim_debugs |= PIM_MASK_PIM_J_P)

#define PIM_DONT_DEBUG_PIM_EVENTS          (qpim_debugs &= ~PIM_MASK_PIM_EVENTS)
#define PIM_DONT_DEBUG_PIM_PACKETS         (qpim_debugs &= ~PIM_MASK_PIM_PACKETS)
#define PIM_DONT_DEBUG_PIM_PACKETDUMP_SEND (qpim_debugs &= ~PIM_MASK_PIM_PACKETDUMP_SEND)
#define PIM_DONT_DEBUG_PIM_PACKETDUMP_RECV (qpim_debugs &= ~PIM_MASK_PIM_PACKETDUMP_RECV)
#define PIM_DONT_DEBUG_PIM_TRACE           (qpim_debugs &= ~PIM_MASK_PIM_TRACE)
#define PIM_DONT_DEBUG_IGMP_EVENTS         (qpim_debugs &= ~PIM_MASK_IGMP_EVENTS)
#define PIM_DONT_DEBUG_IGMP_PACKETS        (qpim_debugs &= ~PIM_MASK_IGMP_PACKETS)
#define PIM_DONT_DEBUG_IGMP_TRACE          (qpim_debugs &= ~PIM_MASK_IGMP_TRACE)
#define PIM_DONT_DEBUG_ZEBRA               (qpim_debugs &= ~PIM_MASK_ZEBRA)
#define PIM_DONT_DEBUG_SSMPINGD            (qpim_debugs &= ~PIM_MASK_SSMPINGD)
#define PIM_DONT_DEBUG_MROUTE              (qpim_debugs &= ~PIM_MASK_MROUTE)
#define PIM_DONT_DEBUG_PIM_HELLO           (qpim_debugs &= ~PIM_MASK_PIM_HELLO)
#define PIM_DONT_DEBUG_PIM_J_P             (qpim_debugs &= ~PIM_MASK_PIM_J_P)

void pim_init(void);
void pim_terminate(void);

#endif /* PIMD_H */
