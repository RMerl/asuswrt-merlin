/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * tkhash.h
 * Prototypes for TKIP hash functions.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.; the
 * contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#ifndef _TKHASH_H_
#define _TKHASH_H_

#include <typedefs.h>

#define TKHASH_P1_KEY_SIZE	10	/* size of TKHash Phase1 output, in bytes */
#define TKHASH_P2_KEY_SIZE	16	/* size of TKHash Phase2 output */

extern void tkhash_phase1(uint16 *P1K, const uint8 *TK, const uint8 *TA,
			  uint32 IV32);
extern void tkhash_phase2(uint8 *RC4KEY, const uint8 *TK, const uint16 *P1K,
			  uint16 IV16);

#endif /* _TKHASH_H_ */
