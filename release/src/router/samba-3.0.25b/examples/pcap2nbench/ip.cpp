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

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ip.hpp"

ip::ip(const uint8_t *data, size_t length)
{
  if (length < 20) {
    std::cerr << "Invalid ip packet" << std::endl;
  }

  version = (data[0] >> 4) & 0x0F;
  header_length = (data[0] & 0x0F) * 4;
  dsfield = data[1];
  memcpy(&total_length, data + 2, 2);
  total_length = ntohs(total_length);
  memcpy(&id, data + 4, 2);
  id = ntohs(id);
  flags = (data[6] >> 4) & 0x0F;
  fragment_offset = ((data[6] & 0x0F) << 4) | data[7];
  ttl = data[8];
  protocol = data[9];
  memcpy(&checksum, data + 10, 2);
  checksum = ntohs(checksum);
  memcpy(&source, data + 12, 4);
  memcpy(&destination, data + 16, 4);
}

std::ostream &operator<<(std::ostream &lhs, const ip &rhs)
{
  lhs << "Version: " << (uint32_t)rhs.version << std::endl
      << "Header length: " << (uint32_t)rhs.header_length << std::endl
      << "Differentiated Services Field: " << (uint32_t)rhs.dsfield << std::endl
      << "Total Length: " << (uint32_t)rhs.total_length << std::endl
      << "Identification: " << (uint32_t)rhs.id << std::endl
      << "Flags: " << (uint32_t)rhs.flags << std::endl
      << "Fragment offset: " << (uint32_t)rhs.fragment_offset << std::endl
      << "TTL: " << (uint32_t)rhs.ttl << std::endl
      << "Protocol: " << (uint32_t)rhs.protocol << std::endl
      << "Checksum: " << (uint32_t)rhs.checksum << std::endl
      << "Source: " << inet_ntoa(*((in_addr *)&rhs.source)) << std::endl
      << "Destination: " << inet_ntoa(*((in_addr *)&rhs.destination)) << std::endl;

  return lhs;
}
