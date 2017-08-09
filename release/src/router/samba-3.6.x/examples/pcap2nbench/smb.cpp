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

#include "smb.hpp"

smb::smb(const uint8_t *data, size_t length)
{
  if (length < 36) {
    memset(magic, 0, 4);
    return;
  }

  /* This code assumes Little Endian...  Don't say I didn't warn you */
  memcpy(&size, data + 2, 2);
  memcpy(magic, data + 4, 4);

  command = data[8];

  memcpy(&nt_status, data + 9, 4);

  flags = data[13];

  memcpy(&flags2, data + 14, 2);
  memcpy(&pid_hi, data + 16, 2);
  memcpy(signature, data + 18, 8);
  memcpy(&reserved, data + 26, 2);
  memcpy(&tid, data + 28, 2);
  memcpy(&pid, data + 30, 2);
  memcpy(&uid, data + 32, 2);
  memcpy(&mid, data + 34, 2);
}

std::ostream &operator<<(std::ostream &lhs, const smb &rhs)
{
  lhs << "Magic: ";
  for (int i = 1; i < 4; i++) {
    lhs << rhs.magic[i];
  }
  lhs << std::endl;

  lhs << "Command: " << (uint16_t)rhs.command << std::endl
      << "NT Status: " << rhs.nt_status << std::endl
      << "Flags: " << (uint16_t)rhs.flags << std::endl
      << "Flags2: " << rhs.flags2 << std::endl
      << "Pid Hi: " << rhs.pid_hi << std::endl
      << "Tid: " << rhs.tid << std::endl
      << "Pid: " << rhs.pid << std::endl
      << "Uid: " << rhs.uid << std::endl
      << "Mid: " << rhs.mid << std::endl;

  return lhs;
}
