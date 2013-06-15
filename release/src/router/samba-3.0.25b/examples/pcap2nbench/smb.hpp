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

#ifndef _SMB_HPP
#define _SMB_HPP

#include <iostream>
#include <stdint.h>

struct smb {
  smb(const uint8_t *data, size_t length);

  uint16_t size;

  uint8_t magic[4]; /* 0xff, 'S', 'M', 'B' */
  uint8_t command;
  uint32_t nt_status;
  uint8_t flags;
  uint16_t flags2;
  uint16_t pid_hi;
  uint8_t signature[8];
  uint16_t reserved;
  uint16_t tid;
  uint16_t pid;
  uint16_t uid;
  uint16_t mid;
};

std::ostream &operator<<(std::ostream &lhs, const smb &rhs);

#endif
