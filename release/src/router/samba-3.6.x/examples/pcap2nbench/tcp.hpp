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

#ifndef _TCP_HPP
#define _TCP_HPP

#include <iostream>
#include <stdint.h>

struct tcp {
  tcp(const uint8_t *data, size_t length);

  uint16_t src_port;
  uint16_t dst_port;
  uint32_t seq_number;
  uint32_t ack_number;
  uint8_t length;
  uint16_t flags;
  uint16_t window_size;
  uint16_t checksum;
};

std::ostream &operator<<(std::ostream &lhs, const tcp &rhs);

#endif
