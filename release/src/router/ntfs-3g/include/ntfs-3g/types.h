/*
 * types.h - Misc type definitions not related to on-disk structure.  
 *           Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_TYPES_H
#define _NTFS_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STDINT_H || !HAVE_CONFIG_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

typedef uint8_t  u8;			/* Unsigned types of an exact size */
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;			/* Signed types of an exact size */
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u16 le16;
typedef u32 le32;
typedef u64 le64;

/*
 * Declare sle{16,32,64} to be unsigned because we do not want sign extension
 * on BE architectures.
 */
typedef u16 sle16;
typedef u32 sle32;
typedef u64 sle64;

typedef u16 ntfschar;			/* 2-byte Unicode character type. */
#define UCHAR_T_SIZE_BITS 1

/*
 * Clusters are signed 64-bit values on NTFS volumes.  We define two types, LCN
 * and VCN, to allow for type checking and better code readability.
 */
typedef s64 VCN;
typedef sle64 leVCN;
typedef s64 LCN;
typedef sle64 leLCN;

/*
 * The NTFS journal $LogFile uses log sequence numbers which are signed 64-bit
 * values.  We define our own type LSN, to allow for type checking and better
 * code readability.
 */
typedef s64 LSN;
typedef sle64 leLSN;

/*
 * Cygwin has a collision between our BOOL and <windef.h>'s
 * As long as this file will be included after <windows.h> were fine.
 */
#ifndef _WINDEF_H
/**
 * enum BOOL - These are just to make the code more readable...
 */
typedef enum {
#ifndef FALSE
	FALSE = 0,
#endif
#ifndef NO
	NO = 0,
#endif
#ifndef ZERO
	ZERO = 0,
#endif
#ifndef TRUE
	TRUE = 1,
#endif
#ifndef YES
	YES = 1,
#endif
#ifndef ONE
	ONE = 1,
#endif
} BOOL;
#endif /* defined _WINDEF_H */

/**
 * enum IGNORE_CASE_BOOL -
 */
typedef enum {
	CASE_SENSITIVE = 0,
	IGNORE_CASE = 1,
} IGNORE_CASE_BOOL;

#define STATUS_OK				(0)
#define STATUS_ERROR				(-1)
#define STATUS_RESIDENT_ATTRIBUTE_FILLED_MFT	(-2)
#define STATUS_KEEP_SEARCHING			(-3)
#define STATUS_NOT_FOUND			(-4)

#endif /* defined _NTFS_TYPES_H */

