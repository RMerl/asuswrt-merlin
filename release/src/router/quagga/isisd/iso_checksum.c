/*
 * IS-IS Rout(e)ing protocol - iso_checksum.c
 *                             ISO checksum related routines
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>
#include "iso_checksum.h"
#include "checksum.h"

/*
 * Calculations of the OSI checksum.
 * ISO/IEC 8473 defines the sum as
 *
 *     L
 *  sum  a (mod 255) = 0
 *     1  i
 *
 *     L 
 *  sum (L-i+1)a (mod 255) = 0
 *     1        i
 *
 */

/*
 * Verifies that the checksum is correct.
 * Return 0 on correct and 1 on invalid checksum.
 * Based on Annex C.4 of ISO/IEC 8473
 */

int
iso_csum_verify (u_char * buffer, int len, uint16_t * csum)
{
  u_int16_t checksum;
  u_int32_t c0;
  u_int32_t c1;

  c0 = *csum & 0xff00;
  c1 = *csum & 0x00ff;

  /*
   * If both are zero return correct
   */
  if (c0 == 0 && c1 == 0)
    return 0;

  /*
   * If either, but not both are zero return incorrect
   */
  if (c0 == 0 || c1 == 0)
    return 1;

  /* Offset of checksum from the start of the buffer */
  int offset = (u_char *) csum - buffer;

  checksum = fletcher_checksum(buffer, len, offset);
  if (checksum == *csum)
    return 0;
  return 1;
}
