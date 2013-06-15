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

#include "tcp.hpp"

tcp::tcp(const uint8_t *data, size_t size)
{
  if (size < 18) {
    std::cerr << "Invalid TCP header" << std::endl;
  }

  memcpy(&src_port, data, 2);
  src_port = ntohs(src_port);
  memcpy(&dst_port, data + 2, 2);
  dst_port = ntohs(dst_port);
  memcpy(&seq_number, data + 4, 4);
  seq_number = ntohl(seq_number);
  memcpy(&ack_number, data + 8, 4);
  ack_number = ntohl(ack_number);
  length = ((data[12] & 0xF0) >> 4) * 4;
  flags = ((data[12] & 0x0F) << 8) | data[13];
  memcpy(&window_size, data + 14, 2);
  window_size = ntohs(window_size);
  memcpy(&checksum, data + 16, 2);
  checksum = ntohs(checksum);
}

std::ostream &operator<<(std::ostream &lhs, const tcp &rhs)
{
  lhs << "Source Port: " << rhs.src_port << std::endl
      << "Destination Port: " << rhs.dst_port << std::endl
      << "Sequence Number: " << rhs.seq_number << std::endl
      << "Ack Number: " << rhs.ack_number << std::endl
      << "Length: " << (uint16_t)(rhs.length) << std::endl
      << "Flags: " << rhs.flags << std::endl
      << "Window Size: " << rhs.window_size << std::endl
      << "Checksum: " << rhs.checksum << std::endl;

  return lhs;
}
