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

#include "ethernet.hpp"

ethernet::ethernet(const uint8_t *data, size_t length) {
  if (length < 14) {
    std::cerr << "Invalid ethernet packet" << std::endl;
  }
  memcpy(dst, data, sizeof(dst));
  memcpy(src, data + 6, sizeof(src));
  memcpy(&type, data + 12, sizeof(type));
  type = ntohs(type);
}

std::ostream &operator<<(std::ostream &lhs, const ethernet &rhs)
{
  lhs << "Destination: ";
  for (int i = 0; i < 6; i++) {
    char buf[3];
    sprintf(buf, "%.2x", rhs.dst[i]);
    if (i) lhs << ":";
    lhs << buf;
  }
  lhs << std::endl;

  lhs << "Source: ";
  for (int i = 0; i < 6; i++) {
    char buf[3];
    sprintf(buf, "%.2x", rhs.src[i]);
    if (i) lhs << ":";
    lhs << buf;
  }
  lhs << std::endl;

  lhs << "Type: ";
  char buf[7];
  sprintf(buf, "%.4x", rhs.type);
  lhs << buf << std::endl;

  return lhs;
}
