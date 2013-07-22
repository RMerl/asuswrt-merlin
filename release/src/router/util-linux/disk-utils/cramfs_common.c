/*
 * cramfs_common - cramfs common code
 *
 * Copyright (c) 2008 Roy Peled, the.roy.peled  -at-  gmail.com
 * Copyright (c) 2004-2006 by Michael Holzt, kju -at- fqdn.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <string.h>
#include "cramfs.h"
#include "../include/bitops.h"

uint32_t u32_toggle_endianness(int big_endian, uint32_t what)
{
	return big_endian == HOST_IS_BIG_ENDIAN ? what : swab32(what);
}

void super_toggle_endianness(int big_endian, struct cramfs_super *super)
{
	if (big_endian != HOST_IS_BIG_ENDIAN) {
		super->magic = swab32(super->magic);
		super->size = swab32(super->size);
		super->flags = swab32(super->flags);
		super->future = swab32(super->future);
		super->fsid.crc = swab32(super->fsid.crc);
		super->fsid.edition = swab32(super->fsid.edition);
		super->fsid.blocks = swab32(super->fsid.blocks);
		super->fsid.files = swab32(super->fsid.files);
	}
}

void inode_toggle_endianness(int input_big_endian, int output_big_endian,
			     struct cramfs_inode *inode_in,
			     struct cramfs_inode *inode_out)
{
	if (input_big_endian == output_big_endian) {
		memmove(inode_out, inode_in, sizeof(*inode_out));
	} else {
		unsigned char inode_out_buf[sizeof(*inode_in)];
		unsigned char *inode_in_buf = (unsigned char*)inode_in;

		inode_out_buf[0] = inode_in_buf[1]; /* 16 bit: mode */
		inode_out_buf[1] = inode_in_buf[0];

		inode_out_buf[2] = inode_in_buf[3]; /* 16 bit: uid */
		inode_out_buf[3] = inode_in_buf[2];

		inode_out_buf[4] = inode_in_buf[6]; /* 24 bit: size */
		inode_out_buf[5] = inode_in_buf[5];
		inode_out_buf[6] = inode_in_buf[4];

		inode_out_buf[7] = inode_in_buf[7]; /* 8 bit: gid width */

		/*
		 * Stop the madness! Outlaw C bitfields! They are unportable
		 * and nasty! See for yourself what a mess this is:
		 */
		if (output_big_endian) {
			inode_out_buf[ 8] = ( (inode_in_buf[ 8]&0x3F) << 2 ) |
				( (inode_in_buf[11]&0xC0) >> 6 );

			inode_out_buf[ 9] = ( (inode_in_buf[11]&0x3F) << 2 ) |
				( (inode_in_buf[10]&0xC0) >> 6 );

			inode_out_buf[10] = ( (inode_in_buf[10]&0x3F) << 2 ) |
				( (inode_in_buf[ 9]&0xC0) >> 6 );

			inode_out_buf[11] = ( (inode_in_buf[ 9]&0x3F) << 2 ) |
				( (inode_in_buf[ 8]&0xC0) >> 6 );
		} else {
			inode_out_buf[ 8] = ( (inode_in_buf[ 8]&0xFD) >> 2 ) |
				( (inode_in_buf[11]&0x03) << 6 );

			inode_out_buf[ 9] = ( (inode_in_buf[11]&0xFD) >> 2 ) |
				( (inode_in_buf[10]&0x03) << 6 );

			inode_out_buf[10] = ( (inode_in_buf[10]&0xFD) >> 2 ) |
				( (inode_in_buf[ 9]&0x03) << 6 );

			inode_out_buf[11] = ( (inode_in_buf[ 9]&0xFD) >> 2 ) |
				( (inode_in_buf[ 8]&0x03) << 6 );
		}
		memmove(inode_out, inode_out_buf, sizeof(*inode_out));
	}
}

void inode_to_host(int from_big_endian, struct cramfs_inode *inode_in,
		   struct cramfs_inode *inode_out)
{
	inode_toggle_endianness(from_big_endian, HOST_IS_BIG_ENDIAN, inode_in,
				inode_out);
}

void inode_from_host(int to_big_endian, struct cramfs_inode *inode_in,
		     struct cramfs_inode *inode_out)
{
	inode_toggle_endianness(HOST_IS_BIG_ENDIAN, to_big_endian, inode_in,
				inode_out);
}
