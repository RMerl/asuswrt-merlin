//=========================================================================
// FILENAME	: misc.h
// DESCRIPTION	: Header for misc.c
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SCANNER_MISC_H
#define _SCANNER_MISC_H

typedef unsigned char __u8;
typedef signed char __s8;
typedef unsigned short __u16;
typedef signed short __s16;
typedef unsigned int __u32;
typedef signed int __s32;
#if __WORDSIZE == 64
typedef unsigned long __u64;
typedef signed long __s64;
#else
typedef unsigned long long __u64;
typedef signed long long __s64;
#endif


inline __u16 le16_to_cpu(__u16 le16);
inline __u32 le32_to_cpu(__u32 le32);
inline __u64 le64_to_cpu(__u64 le64);
inline __u8 fget_byte(FILE *fp);
inline __u16 fget_le16(FILE *fp);
inline __u32 fget_le32(FILE *fp);

inline __u32 cpu_to_be32(__u32 cpu32);

extern char * sha1_hex(char *key);

#endif
