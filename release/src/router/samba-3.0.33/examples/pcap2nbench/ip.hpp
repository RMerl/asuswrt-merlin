/*\
 *  pcap2nbench - Converts libpcap network traces to nbench input
 *  Copyright (C) 2004  Jim McDonough <jmcd@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Written by Anthony Liguori <aliguori@us.ibm.com>
\*/

#ifndef IP_HPP
#define IP_HPP

#include <stdint.h>
#include <iostream>

struct ip {
  ip(const uint8_t *data, size_t length);

  uint8_t version;
  uint8_t header_length;
  uint8_t dsfield;
  uint16_t total_length;
  uint16_t id;
  uint8_t flags;
  uint16_t fragment_offset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint32_t source;
  uint32_t destination;
};

std::ostream &operator<<(std::ostream &lhs, const ip &rhs);

#endif
