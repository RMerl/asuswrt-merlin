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

#include "ntcreateandxresponse.hpp"

NtCreateAndXResponse::NtCreateAndXResponse(const uint8_t *data, size_t size)
{
  if (size < 71) {
    return;
  }
   
  word_count = data[0];
  and_x_command = data[1];
  reserved = data[2];
  memcpy(&and_x_offset, data + 3, 2);
  oplock_level = data[5];
  memcpy(&fid, data + 6, 2);
}

std::ostream &operator<<(std::ostream &lhs, const NtCreateAndXResponse &rhs)
{
  return lhs;
}
