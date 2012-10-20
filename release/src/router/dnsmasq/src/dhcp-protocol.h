/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_ALTPORT 1067
#define DHCP_CLIENT_ALTPORT 1068
#define PXE_PORT 4011

#define BOOTREQUEST              1
#define BOOTREPLY                2
#define DHCP_COOKIE              0x63825363

/* The Linux in-kernel DHCP client silently ignores any packet 
   smaller than this. Sigh...........   */
#define MIN_PACKETSZ             300

#define OPTION_PAD               0
#define OPTION_NETMASK           1
#define OPTION_ROUTER            3
#define OPTION_DNSSERVER         6
#define OPTION_HOSTNAME          12
#define OPTION_DOMAINNAME        15
#define OPTION_BROADCAST         28
#define OPTION_VENDOR_CLASS_OPT  43
#define OPTION_REQUESTED_IP      50 
#define OPTION_LEASE_TIME        51
#define OPTION_OVERLOAD          52
#define OPTION_MESSAGE_TYPE      53
#define OPTION_SERVER_IDENTIFIER 54
#define OPTION_REQUESTED_OPTIONS 55
#define OPTION_MESSAGE           56
#define OPTION_MAXMESSAGE        57
#define OPTION_T1                58
#define OPTION_T2                59
#define OPTION_VENDOR_ID         60
#define OPTION_CLIENT_ID         61
#define OPTION_SNAME             66
#define OPTION_FILENAME          67
#define OPTION_USER_CLASS        77
#define OPTION_CLIENT_FQDN       81
#define OPTION_AGENT_ID          82
#define OPTION_ARCH              93
#define OPTION_PXE_UUID          97
#define OPTION_SUBNET_SELECT     118
#define OPTION_DOMAIN_SEARCH     119
#define OPTION_SIP_SERVER        120
#define OPTION_VENDOR_IDENT      124
#define OPTION_VENDOR_IDENT_OPT  125
#define OPTION_END               255

#define SUBOPT_CIRCUIT_ID        1
#define SUBOPT_REMOTE_ID         2
#define SUBOPT_SUBNET_SELECT     5     /* RFC 3527 */
#define SUBOPT_SUBSCR_ID         6     /* RFC 3393 */
#define SUBOPT_SERVER_OR         11    /* RFC 5107 */

#define SUBOPT_PXE_BOOT_ITEM     71    /* PXE standard */
#define SUBOPT_PXE_DISCOVERY     6
#define SUBOPT_PXE_SERVERS       8
#define SUBOPT_PXE_MENU          9
#define SUBOPT_PXE_MENU_PROMPT   10

#define DHCPDISCOVER             1
#define DHCPOFFER                2
#define DHCPREQUEST              3
#define DHCPDECLINE              4
#define DHCPACK                  5
#define DHCPNAK                  6
#define DHCPRELEASE              7
#define DHCPINFORM               8

#define BRDBAND_FORUM_IANA       3561 /* Broadband forum IANA enterprise */

#define DHCP_CHADDR_MAX 16

struct dhcp_packet {
  u8 op, htype, hlen, hops;
  u32 xid;
  u16 secs, flags;
  struct in_addr ciaddr, yiaddr, siaddr, giaddr;
  u8 chaddr[DHCP_CHADDR_MAX], sname[64], file[128];
  u8 options[312];
};
