/*
 * macros to convert each packed bitfield structure from little endian to big
 * endian and vice versa.  These are needed when creating a filesystem on a
 * machine with different byte ordering to the target architecture.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007
 * Phillip Lougher <phillip@lougher.demon.co.uK>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * mksquashfs.h
 *
 */

/*
 * macros used to swap each structure entry, taking into account
 * bitfields and different bitfield placing conventions on differing architectures
 */
#if __BYTE_ORDER == __BIG_ENDIAN
	/* convert from big endian to little endian */
#define SQUASHFS_SWAP(value, p, pos, tbits) _SQUASHFS_SWAP(value, p, pos, tbits, b_pos)
#else
	/* convert from little endian to big endian */ 
#define SQUASHFS_SWAP(value, p, pos, tbits) _SQUASHFS_SWAP(value, p, pos, tbits, 64 - tbits - b_pos)
#endif

#define _SQUASHFS_SWAP(value, p, pos, tbits, SHIFT) {\
	int bits;\
	int b_pos = pos % 8;\
	unsigned long long val = (long long) value << (SHIFT);\
	unsigned char *s = ((unsigned char *) &val) + 7;\
	unsigned char *d = ((unsigned char *)p) + (pos / 8);\
	for(bits = 0; bits < (tbits + b_pos); bits += 8) \
		*d++ |= *s--;\
}
#define SQUASHFS_MEMSET(s, d, n)	memset(d, 0, n);
