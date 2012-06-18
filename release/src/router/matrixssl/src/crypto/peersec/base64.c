/*
 *	base64.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Base64 operations
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include "../cryptoLayer.h"

static const unsigned char map[256] = {
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
 52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
255, 254, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
  7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
 19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
 37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
 49,  50,  51, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255 };

int32 ps_base64_decode(const unsigned char *in,  uint32 len, 
					unsigned char *out, uint32 *outlen)
{
	unsigned long	t, x, y, z;
	unsigned char	c;
	int32				g;

	if (in == NULL || out == NULL || outlen == NULL) {
		return -1;
	}
	g = 3;
	for (x = y = z = t = 0; x < len; x++) {
		c = map[in[x]&0xFF];
		if (c == 255) {
			continue;
		}
/*
	the final = symbols are read and used to trim the remaining bytes
 */
	if (c == 254) {
		c = 0;
/*
		prevent g < 0 which would potentially allow an overflow later
 */
		if (--g < 0) {
			return CRYPT_INVALID_PACKET;
		}
	} else if (g != 3) {
/*
		we only allow = to be at the end
 */
		return CRYPT_INVALID_PACKET;
	}

		t = (t<<6)|c;

		if (++y == 4) {
			if (z + g > *outlen) { 
				return CRYPT_BUFFER_OVERFLOW;
			}
			out[z++] = (unsigned char)((t>>16)&255);
			if (g > 1) {
				out[z++] = (unsigned char)((t>>8)&255);
			}
			if (g > 2) {
				out[z++] = (unsigned char)(t&255);
			}
			y = t = 0;
		}
	}
	if (y != 0) {
		return -1;
	}
	*outlen = z;
	return 0;
}

/******************************************************************************/

