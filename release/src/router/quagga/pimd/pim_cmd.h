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

#ifndef PIM_CMD_H
#define PIM_CMD_H

#define PIM_STR                                "PIM information\n"
#define IGMP_STR                               "IGMP information\n"
#define IGMP_GROUP_STR                         "IGMP groups information\n"
#define IGMP_SOURCE_STR                        "IGMP sources information\n"
#define CONF_SSMPINGD_STR                      "Enable ssmpingd operation\n"
#define SHOW_SSMPINGD_STR                      "ssmpingd operation\n"
#define IFACE_PIM_STR                          "Enable PIM SSM operation\n"
#define IFACE_IGMP_STR                         "Enable IGMP operation\n"
#define IFACE_IGMP_QUERY_INTERVAL_STR          "IGMP host query interval\n"
#define IFACE_IGMP_QUERY_MAX_RESPONSE_TIME_STR      "IGMP max query response value (seconds)\n"
#define IFACE_IGMP_QUERY_MAX_RESPONSE_TIME_DSEC_STR "IGMP max query response value (deciseconds)\n"
#define DEBUG_IGMP_STR                              "IGMP protocol activity\n"
#define DEBUG_IGMP_EVENTS_STR                       "IGMP protocol events\n"
#define DEBUG_IGMP_PACKETS_STR                      "IGMP protocol packets\n"
#define DEBUG_IGMP_TRACE_STR                        "IGMP internal daemon activity\n"
#define DEBUG_MROUTE_STR                            "PIM interaction with kernel MFC cache\n"
#define DEBUG_PIM_STR                               "PIM protocol activity\n"
#define DEBUG_PIM_EVENTS_STR                        "PIM protocol events\n"
#define DEBUG_PIM_PACKETS_STR                       "PIM protocol packets\n"
#define DEBUG_PIM_HELLO_PACKETS_STR                 "PIM Hello protocol packets\n"
#define DEBUG_PIM_J_P_PACKETS_STR                   "PIM Join/Prune protocol packets\n"
#define DEBUG_PIM_PACKETDUMP_STR                    "PIM packet dump\n"
#define DEBUG_PIM_PACKETDUMP_SEND_STR               "Dump sent packets\n"
#define DEBUG_PIM_PACKETDUMP_RECV_STR               "Dump received packets\n"
#define DEBUG_PIM_TRACE_STR                         "PIM internal daemon activity\n"
#define DEBUG_PIM_ZEBRA_STR                         "ZEBRA protocol activity\n"
#define DEBUG_SSMPINGD_STR                          "ssmpingd activity\n"
#define CLEAR_IP_IGMP_STR                           "IGMP clear commands\n"
#define CLEAR_IP_PIM_STR                            "PIM clear commands\n"
#define MROUTE_STR                                  "IP multicast routing table\n"
#define RIB_STR                                     "IP unicast routing table\n"

#define PIM_CMD_NO                                   "no"
#define PIM_CMD_IP_MULTICAST_ROUTING                 "ip multicast-routing"
#define PIM_CMD_IP_IGMP_QUERY_INTERVAL               "ip igmp query-interval"
#define PIM_CMD_IP_IGMP_QUERY_MAX_RESPONSE_TIME      "ip igmp query-max-response-time"
#define PIM_CMD_IP_IGMP_QUERY_MAX_RESPONSE_TIME_DSEC "ip igmp query-max-response-time-dsec"

void pim_cmd_init(void);

#endif /* PIM_CMD_H */
