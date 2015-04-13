/* dnsmasq is Copyright (c) 2000-2015 Simon Kelley

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

#define ALL_NODES                 "FF02::1"
#define ALL_ROUTERS               "FF02::2"

struct ping_packet {
  u8 type, code;
  u16 checksum;
  u16 identifier;
  u16 sequence_no;
};

struct ra_packet {
  u8 type, code;
  u16 checksum;
  u8 hop_limit, flags;
  u16 lifetime;
  u32 reachable_time;
  u32 retrans_time;
};

struct neigh_packet {
  u8 type, code;
  u16 checksum;
  u16 reserved;
  struct in6_addr target;
};

struct prefix_opt {
  u8 type, len, prefix_len, flags;
  u32 valid_lifetime, preferred_lifetime, reserved;
  struct in6_addr prefix;
};

#define ICMP6_OPT_SOURCE_MAC   1
#define ICMP6_OPT_PREFIX       3
#define ICMP6_OPT_MTU          5
#define ICMP6_OPT_ADV_INTERVAL 7
#define ICMP6_OPT_RT_INFO     24
#define ICMP6_OPT_RDNSS       25
#define ICMP6_OPT_DNSSL       31



