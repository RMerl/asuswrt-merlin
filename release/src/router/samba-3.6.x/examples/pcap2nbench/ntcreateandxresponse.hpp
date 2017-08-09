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

#ifndef _NT_CREATE_AND_X_RESPONSE_HPP
#define _NT_CREATE_AND_X_RESPONSE_HPP

#include <iostream>
#include <stdint.h>

struct NtCreateAndXResponse {
  enum {
    COMMAND = 0xa2
  };

  NtCreateAndXResponse() {} 
  NtCreateAndXResponse(const uint8_t *data, size_t size);

  uint8_t word_count;
  uint8_t and_x_command;
  uint8_t reserved;
  uint16_t and_x_offset;
  uint8_t oplock_level;
  uint16_t fid;
  uint32_t create_action;
  uint64_t create_date;
  uint64_t access_date;
  uint64_t write_date;
  uint64_t change_date;
  uint32_t file_attributes;
  uint64_t allocation_size;
  uint64_t end_of_file;
  uint16_t file_type;
  uint16_t ipc_state;
  uint8_t is_directory;
  uint16_t byte_count;
};

std::ostream &operator<<(std::ostream &lhs, const NtCreateAndXResponse &rhs);

#endif
