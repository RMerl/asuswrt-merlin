/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef DPROXY_H
#define DPROXY_H

#define PORT 53
#define NAME_SIZE 256		/* 255--> 256 CMC: 2001/12/27 */
#define MAX_PACKET_SIZE 512
#define BUF_SIZE NAME_SIZE

typedef u_int16_t uint16;
typedef u_int32_t uint32;

#include "dns.h"

#ifndef DNS_TIMEOUT
#define DNS_TIMEOUT 240
#endif
#ifndef NAME_SERVER_DEFAULT
//#define NAME_SERVER_DEFAULT "198.95.251.10"
//#define NAME_SERVER_DEFAULT "140.113.1.1"
//JYWeng 20031215:
#define NAME_SERVER_DEFAULT "168.95.1.1"
#endif
#ifndef CONFIG_FILE_DEFAULT
#define CONFIG_FILE_DEFAULT "/etc/dproxy.conf"
#endif
#ifndef RESOLV_FILE_DEFAULT
#define RESOLV_FILE_DEFAULT "/etc/resolv.conf"
#endif
#ifndef DENY_FILE_DEFAULT
#define DENY_FILE_DEFAULT "/etc/dproxy.deny"
#endif
#ifndef CACHE_FILE_DEFAULT
#define CACHE_FILE_DEFAULT "/var/cache/dproxy.cache"
#endif
#ifndef HOSTS_FILE_DEFAULT
#define HOSTS_FILE_DEFAULT "/etc/hosts"
#endif
#ifndef PURGE_TIME_DEFAULT
//#define PURGE_TIME_DEFAULT 48 * 60 * 60
#define PURGE_TIME_DEFAULT (60 * 60)
#endif
#ifndef PPP_DEV_DEFAULT
#define PPP_DEV_DEFAULT "/var/run/ppp0.pid"
#endif
#ifndef DHCP_LEASES_DEFAULT
#define DHCP_LEASES_DEFAULT "/var/state/dhcp/dhcpd.leases"
#endif
#ifndef PPP_DETECT_DEFAULT
//#define PPP_DETECT_DEFAULT 1
#define PPP_DETECT_DEFAULT 0
#endif
#ifndef DEBUG_FILE_DEFAULT
#define DEBUG_FILE_DEFAULT "/var/log/dproxy.debug.log"
#endif

/* Added by CMC 8/4/2001 */
#define DNS_DEBUG		1
#define DNS_TICK_TIME		10	/* sec. */
#define CACHE_CHECK_TIME	30	/* sec. */
#define MAX_CACHE_NUM		400	/* 400 cache names */
#define MAX_LIST_SIZE		150	/* 908k = 6052 * 150 */
#define MAX_NS_COUNT		8

void debug_perror( char * msg );
void debug(char *fmt, ...);

#endif
