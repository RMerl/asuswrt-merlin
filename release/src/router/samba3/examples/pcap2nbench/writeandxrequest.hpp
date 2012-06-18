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

#ifndef _WRITE_AND_X_REQUEST_HPP
#define _WRITE_AND_X_REQUEST_HPP

#include <iostream>
#include <stdint.h>

struct WriteAndXRequest {
  enum {
    COMMAND = 0x2f
  };

  WriteAndXRequest() {} 
  WriteAndXRequest(const uint8_t *data, size_t size);

  uint8_t word_count;
  uint8_t and_x_command;
  uint8_t reserved;
  uint16_t and_x_offset;
  uint16_t fid;
  uint32_t offset;
  uint32_t reserved1;
  uint16_t write_mode;
  uint16_t remaining;
  uint16_t data_length_hi;
  uint16_t data_length_lo;
  uint16_t data_offset;
  uint32_t high_offset;
  uint16_t byte_count;
};

std::ostream &operator<<(std::ostream &lhs, const WriteAndXRequest &rhs);

#endif
