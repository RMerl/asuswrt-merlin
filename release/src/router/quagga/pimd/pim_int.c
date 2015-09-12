/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <string.h>
#include <netinet/in.h>

#include "pim_int.h"

uint32_t pim_read_uint32_host(const uint8_t *buf)
{
  uint32_t val;
  memcpy(&val, buf, sizeof(val));
  /* val is in netorder */
  val = ntohl(val);
  /* val is in hostorder */
  return val;
}

void pim_write_uint32(uint8_t *buf, uint32_t val_host)
{
  /* val_host is in host order */
  val_host = htonl(val_host);
  /* val_host is in netorder */
  memcpy(buf, &val_host, sizeof(val_host));
}
