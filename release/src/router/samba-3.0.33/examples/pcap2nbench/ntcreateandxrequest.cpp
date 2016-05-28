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

#include "ntcreateandxrequest.hpp"

NtCreateAndXRequest::NtCreateAndXRequest(const uint8_t *data, size_t size)
{
  if (size < 52) {
    std::cerr << "Invalid NtCreateAndX Request" << std::endl;
    return;
  }

  word_count = data[0];
  and_x_command = data[1];
  reserved = data[2];
  memcpy(&and_x_offset, data + 3, 2);
  reserved1 = data[5];
  memcpy(&file_name_len, data + 6, 2);
  memcpy(&create_flags, data + 8, 4);
  memcpy(&root_fid, data + 12, 4);
  memcpy(&access_mask, data + 16, 4);
  memcpy(&allocation_size, data + 20, 8);
  memcpy(&file_attributes, data + 28, 4);
  memcpy(&share_access, data + 32, 4);
  memcpy(&disposition, data + 36, 4);
  memcpy(&create_options, data + 40, 4);
  memcpy(&impersonation, data + 44, 4);
  security_flags = data[48];
  memcpy(&byte_count, data + 49, 2);
  file_name = (const char *)(data + 51);
}

std::ostream &operator<<(std::ostream &lhs, const NtCreateAndXRequest &rhs)
{
  lhs << "Word Count: " << (uint16_t)rhs.word_count << std::endl
      << "AndXCommand: " << (uint16_t)rhs.and_x_command << std::endl
      << "Reserved: " << (uint16_t)rhs.reserved << std::endl
      << "AndXOffset: " << rhs.and_x_offset << std::endl
      << "Reserved: " << (uint16_t)rhs.reserved1 << std::endl
      << "File Name Len: " << rhs.file_name_len << std::endl
      << "Create Flags: " << rhs.create_flags << std::endl
      << "Root FID: " << rhs.root_fid << std::endl
      << "Access Mask: " << rhs.access_mask << std::endl
      << "Allocation Size: " << rhs.allocation_size << std::endl
      << "File Attributes: " << rhs.file_attributes << std::endl
      << "Share Access: " << rhs.share_access << std::endl
      << "Disposition: " << rhs.disposition << std::endl
      << "Create Options: " << rhs.create_options << std::endl
      << "Impersonation: " << rhs.impersonation << std::endl
      << "Security Flags: " << (uint16_t)rhs.security_flags << std::endl
      << "Byte Count: " << rhs.byte_count << std::endl
      << "File Name: " << rhs.file_name << std::endl;
  return lhs;
}
