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

#define DHCPV6_SERVER_PORT 547
#define DHCPV6_CLIENT_PORT 546

#define ALL_SERVERS                  "FF05::1:3"
#define ALL_RELAY_AGENTS_AND_SERVERS "FF02::1:2"

#define DHCP6SOLICIT      1
#define DHCP6ADVERTISE    2
#define DHCP6REQUEST      3
#define DHCP6CONFIRM      4
#define DHCP6RENEW        5
#define DHCP6REBIND       6
#define DHCP6REPLY        7
#define DHCP6RELEASE      8
#define DHCP6DECLINE      9
#define DHCP6RECONFIGURE  10
#define DHCP6IREQ         11
#define DHCP6RELAYFORW    12
#define DHCP6RELAYREPL    13

#define OPTION6_CLIENT_ID       1
#define OPTION6_SERVER_ID       2
#define OPTION6_IA_NA           3
#define OPTION6_IA_TA           4
#define OPTION6_IAADDR          5
#define OPTION6_ORO             6
#define OPTION6_PREFERENCE      7
#define OPTION6_ELAPSED_TIME    8
#define OPTION6_RELAY_MSG       9
#define OPTION6_AUTH            11
#define OPTION6_UNICAST         12
#define OPTION6_STATUS_CODE     13
#define OPTION6_RAPID_COMMIT    14
#define OPTION6_USER_CLASS      15
#define OPTION6_VENDOR_CLASS    16
#define OPTION6_VENDOR_OPTS     17
#define OPTION6_INTERFACE_ID    18
#define OPTION6_RECONFIGURE_MSG 19
#define OPTION6_RECONF_ACCEPT   20
#define OPTION6_DNS_SERVER      23
#define OPTION6_DOMAIN_SEARCH   24
#define OPTION6_REMOTE_ID       37
#define OPTION6_SUBSCRIBER_ID   38
#define OPTION6_FQDN            39

#define DHCP6SUCCESS     0
#define DHCP6UNSPEC      1
#define DHCP6NOADDRS     2
#define DHCP6NOBINDING   3
#define DHCP6NOTONLINK   4
#define DHCP6USEMULTI    5

