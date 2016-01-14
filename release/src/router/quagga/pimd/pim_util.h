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

#ifndef PIM_UTIL_H
#define PIM_UTIL_H

#include <stdint.h>

#include <zebra.h>

#include "checksum.h"

uint8_t igmp_msg_encode16to8(uint16_t value);
uint16_t igmp_msg_decode8to16(uint8_t code);

void pim_pkt_dump(const char *label, const uint8_t *buf, int size);

#endif /* PIM_UTIL_H */
