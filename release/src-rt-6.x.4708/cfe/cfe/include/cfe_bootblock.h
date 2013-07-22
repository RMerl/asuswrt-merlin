/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Boot Block				File: cfe_bootblock.h
    *  
    *  The structure of the boot block used on block-style devices
    *  like disks and CDROMs
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


/*
 * CFE boot block, modeled loosely on Alpha.
 *
 * It consists of:
 *
 * 		BSD disk label
 *		<blank space>
 *		Boot block info (5 u_int_64s)
 *
 * The boot block portion looks like:
 *
 *
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *      |                        BOOT_MAGIC_NUMBER                      |
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *      | Flags |   Reserved    | Vers  |      Header Checksum          |
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *      |             Secondary Loader Location (bytes)                 |
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *      |     Loader Checksum           |     Size of loader (bytes)    |
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *      |          Reserved             |    Architecture Information   |
 *      +-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Boot block fields should always be read as 64-bit numbers.
 *
 */


struct boot_block {
	uint64_t bb_data[64];		/* data (disklabel, also as below) */
};
#define bb_magic	bb_data[59]	/* magic number */
#define bb_hdrinfo	bb_data[60]	/* header checksum, ver, flags */
#define bb_secstart	bb_data[61]	/* secondary start (bytes) */
#define bb_secsize	bb_data[62]	/* secondary size (bytes) */
#define bb_archinfo	bb_data[63]	/* architecture info */

#define	BOOT_BLOCK_OFFSET	0	/* offset of boot block. */
#define	BOOT_BLOCK_BLOCKSIZE	512	/* block size for sec. size/start,
					 * and for boot block itself
					 */
#define BOOT_BLOCK_SIZE		40	/* 5 64-bit words */

/* 
 * This is the highest block number that we look at when 
 * searching for a valid boot block
 */
#define BOOT_BLOCK_MAXLOC	16	

/*
 * Fields within the boot block
 */
#define _U64(x) ((uint64_t) x##LL)
#define BOOT_MAGIC_NUMBER	_U64(0x43465631424f4f54)
#define BOOT_HDR_CHECKSUM_MASK	_U64(0x00000000FFFFFFFF)
#define BOOT_HDR_VER_MASK       _U64(0x000000FF00000000)
#define BOOT_HDR_VER_SHIFT	32
#define BOOT_HDR_FLAGS_MASK	_U64(0xFF00000000000000)
#define BOOT_SECSIZE_MASK	_U64(0x00000000FFFFFFFF)
#define BOOT_DATA_CHECKSUM_MASK _U64(0xFFFFFFFF00000000)
#define BOOT_DATA_CHECKSUM_SHIFT 32
#define BOOT_ARCHINFO_MASK	_U64(0x00000000FFFFFFFF)

#define BOOT_HDR_VERSION	1

#define	CHECKSUM_BOOT_DATA(data,len,cksum)				\
	do {								\
		uint32_t *_ptr = (uint32_t *) (data);                 \
		uint32_t _cksum;					\
		int _i;							\
									\
		_cksum = 0;						\
		for (_i = 0;						\
		    _i < ((len) / sizeof (uint32_t));       		\
		    _i++)						\
			_cksum += _ptr[_i];				\
		*(cksum) =  _cksum;					\
	} while (0)
