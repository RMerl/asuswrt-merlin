/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#ifndef HOTPLUG2_UTILS_H
#define HOTPLUG2_UTILS_H 1

#include "hotplug2.h"

#define NETLINK_UNDEFINED	0
#define NETLINK_CONNECT		1
#define NETLINK_BIND		2

inline event_seqnum_t get_kernel_seqnum();
inline int init_netlink_socket(int);

#endif
