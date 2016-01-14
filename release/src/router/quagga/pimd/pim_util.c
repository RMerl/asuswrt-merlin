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

#include <zebra.h>

#include "log.h"

#include "pim_util.h"

/*
  RFC 3376: 4.1.7. QQIC (Querier's Query Interval Code)
  
  If QQIC < 128,  QQI = QQIC
  If QQIC >= 128, QQI = (mant | 0x10) << (exp + 3)
  
  0 1 2 3 4 5 6 7
  +-+-+-+-+-+-+-+-+
  |1| exp | mant  |
  +-+-+-+-+-+-+-+-+
  
  Since exp=0..7 then (exp+3)=3..10, then QQI has
  one of the following bit patterns:
  
  exp=0: QQI = 0000.0000.1MMM.M000
  exp=1: QQI = 0000.0001.MMMM.0000
  ...
  exp=6: QQI = 001M.MMM0.0000.0000
  exp=7: QQI = 01MM.MM00.0000.0000
  --------- ---------
  0x4  0x0  0x0  0x0
*/
uint8_t igmp_msg_encode16to8(uint16_t value)
{
  uint8_t code;

  if (value < 128) {
    code = value;
  }
  else {
    uint16_t mask = 0x4000;
    uint8_t  exp;
    uint16_t mant;
    for (exp = 7; exp > 0; --exp) {
      if (mask & value)
	break;
      mask >>= 1;
    }
    mant = 0x000F & (value >> (exp + 3));
    code = ((uint8_t) 1 << 7) | ((uint8_t) exp << 4) | (uint8_t) mant;
  }

  return code;
}

/*
  RFC 3376: 4.1.7. QQIC (Querier's Query Interval Code)
  
  If QQIC < 128,  QQI = QQIC
  If QQIC >= 128, QQI = (mant | 0x10) << (exp + 3)
  
  0 1 2 3 4 5 6 7
  +-+-+-+-+-+-+-+-+
  |1| exp | mant  |
  +-+-+-+-+-+-+-+-+
*/
uint16_t igmp_msg_decode8to16(uint8_t code)
{
  uint16_t value;

  if (code < 128) {
    value = code;
  }
  else {
    uint16_t mant = (code & 0x0F);
    uint8_t  exp  = (code & 0x70) >> 4;
    value = (mant | 0x10) << (exp + 3);
  }

  return value;
}

void pim_pkt_dump(const char *label, const uint8_t *buf, int size)
{
  char dump_buf[1000];
  int i = 0;
  int j = 0;

  for (; i < size; ++i, j += 2) {
    int left = sizeof(dump_buf) - j;
    if (left < 4) {
      if (left > 1) {
	strcat(dump_buf + j, "!"); /* mark as truncated */
      }
      break;
    }
    snprintf(dump_buf + j, left, "%02x", buf[i]);
  }

  zlog_debug("%s: pkt dump size=%d: %s",
	     label,
	     size,
	     dump_buf);
}
