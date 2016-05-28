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

#include "closerequest.hpp"

CloseRequest::CloseRequest(const uint8_t *data, size_t size)
{
  if (size < 9) {
    std::cerr << "Invalid Close Request" << std::endl;
    return;
  }

  word_count = data[0];
  memcpy(&fid, data + 1, 2);
  memcpy(&last_write, data + 3, 4);
  memcpy(&byte_count, data + 7, 2);
}

std::ostream &operator<<(std::ostream &lhs, const CloseRequest &rhs)
{
  lhs << "Word Count: " << (uint16_t)rhs.word_count << std::endl
      << "Fid: " << rhs.fid << std::endl
      << "Last Write: " << rhs.last_write << std::endl
      << "Byte Count: " << rhs.byte_count << std::endl;
  return lhs;
}
