/*\
 *  pcap2nbench - Converts libpcap network traces to nbench input
 *  Copyright (C) 2004  Jim McDonough <jmcd@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Written by Anthony Liguori <aliguori@us.ibm.com>
\*/

#include <netinet/in.h>

#include "writeandxrequest.hpp"

WriteAndXRequest::WriteAndXRequest(const uint8_t *data, size_t size)
{
  if (size < 31) {
    std::cerr << "Invalid WriteAndX Request" << std::endl;
    return;
  }
  word_count = data[0];
  and_x_command = data[1];
  reserved = data[2];
  memcpy(&and_x_offset, data + 3, 2);
  memcpy(&fid, data + 5, 2);
  memcpy(&offset, data + 7, 4);
  memcpy(&reserved1, data + 11, 4);
  memcpy(&write_mode, data + 15, 2);
  memcpy(&remaining, data + 17, 2);
  memcpy(&data_length_hi, data + 19, 2);
  memcpy(&data_length_lo, data + 21, 2);
  memcpy(&data_offset, data + 23, 2);
  memcpy(&high_offset, data + 25, 4);
  memcpy(&byte_count, data + 29, 2);

}

std::ostream &operator<<(std::ostream &lhs, const WriteAndXRequest &rhs)
{
  lhs << "Word Count: " << (uint16_t)rhs.word_count << std::endl
      << "AndXCommand: " << (uint16_t)rhs.and_x_command << std::endl
      << "Reserved: " << (uint16_t)rhs.reserved << std::endl
      << "AndX Offset: " << rhs.and_x_offset << std::endl
      << "Fid: " << rhs.fid << std::endl
      << "Offset: " << rhs.offset << std::endl
      << "Reserved: " << rhs.reserved1 << std::endl
      << "Write Mode: " << rhs.write_mode << std::endl
      << "Remaining: " << rhs.remaining << std::endl
      << "Data Length Hi: " << rhs.data_length_hi << std::endl
      << "Data Length Lo: " << rhs.data_length_lo << std::endl
      << "Data Offset: " << rhs.data_offset << std::endl
      << "High Offset: " << rhs.high_offset << std::endl
      << "Byte Count: " << rhs.byte_count << std::endl;
  return lhs;
}
