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

#include "readandxrequest.hpp"

ReadAndXRequest::ReadAndXRequest(const uint8_t *data, size_t size)
{
  if (size < 27) {
    std::cerr << "Invalid ReadAndX Request" << std::endl;
    return;
  }
  word_count = data[0];
  and_x_command = data[1];
  reserved = data[2];
  memcpy(&and_x_offset, data + 3, 2);
  memcpy(&fid, data + 5, 2);
  memcpy(&offset, data + 7, 4);
  memcpy(&max_count_low, data + 11, 2);
  memcpy(&min_count, data + 13, 2);
  memcpy(&max_count_high, data + 15, 4);
  memcpy(&remaining, data + 19, 2);
  memcpy(&high_offset, data + 21, 4);
  memcpy(&byte_count, data + 25, 2);
}

std::ostream &operator<<(std::ostream &lhs, const ReadAndXRequest &rhs)
{
  lhs << "Word Count: " << (uint16_t)rhs.word_count << std::endl
      << "AndXCommand: " << (uint16_t)rhs.and_x_command << std::endl
      << "Reserved: " << (uint16_t)rhs.reserved << std::endl
      << "AndX Offset: " << rhs.and_x_offset << std::endl
      << "Fid: " << rhs.fid << std::endl
      << "Offset: " << rhs.offset << std::endl
      << "Max Count Low: " << rhs.max_count_low << std::endl
      << "Min Count: " << rhs.min_count << std::endl
      << "Max Count High: " << rhs.max_count_high << std::endl
      << "Remaining: " << rhs.remaining << std::endl
      << "High Offset: " << rhs.high_offset << std::endl
      << "Byte Count: " << rhs.byte_count << std::endl;
  return lhs;
}
