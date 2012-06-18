/*
 * endians.h - Definitions related to handling of byte ordering. 
 *             Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2005 Anton Altaparmakov
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

#ifndef _NTFS_ENDIANS_H
#define _NTFS_ENDIANS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Notes:
 *	We define the conversion functions including typecasts since the
 * defaults don't necessarily perform appropriate typecasts.
 *	Also, using our own functions means that we can change them if it
 * turns out that we do need to use the unaligned access macros on
 * architectures requiring aligned memory accesses...
 */

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif
#ifdef HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef __BYTE_ORDER
#	if defined(_BYTE_ORDER)
#		define __BYTE_ORDER _BYTE_ORDER
#		define __LITTLE_ENDIAN _LITTLE_ENDIAN
#		define __BIG_ENDIAN _BIG_ENDIAN
#	elif defined(BYTE_ORDER)
#		define __BYTE_ORDER BYTE_ORDER
#		define __LITTLE_ENDIAN LITTLE_ENDIAN
#		define __BIG_ENDIAN BIG_ENDIAN
#	elif defined(__BYTE_ORDER__)
#		define __BYTE_ORDER __BYTE_ORDER__
#		define __LITTLE_ENDIAN __LITTLE_ENDIAN__
#		define __BIG_ENDIAN __BIG_ENDIAN__
#	elif (defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)) || \
			defined(WORDS_LITTLEENDIAN)
#		define __BYTE_ORDER 1
#		define __LITTLE_ENDIAN 1
#		define __BIG_ENDIAN 0
#	elif (!defined(_LITTLE_ENDIAN) && defined(_BIG_ENDIAN)) || \
			defined(WORDS_BIGENDIAN)
#		define __BYTE_ORDER 0
#		define __LITTLE_ENDIAN 1
#		define __BIG_ENDIAN 0
#	else
#		error "__BYTE_ORDER is not defined."
#	endif
#endif

#define __ntfs_bswap_constant_16(x)		\
	  (u16)((((u16)(x) & 0xff00) >> 8) |	\
		(((u16)(x) & 0x00ff) << 8))

#define __ntfs_bswap_constant_32(x)			\
	  (u32)((((u32)(x) & 0xff000000u) >> 24) |	\
		(((u32)(x) & 0x00ff0000u) >>  8) |	\
		(((u32)(x) & 0x0000ff00u) <<  8) |	\
		(((u32)(x) & 0x000000ffu) << 24))

#define __ntfs_bswap_constant_64(x)				\
	  (u64)((((u64)(x) & 0xff00000000000000ull) >> 56) |	\
		(((u64)(x) & 0x00ff000000000000ull) >> 40) |	\
		(((u64)(x) & 0x0000ff0000000000ull) >> 24) |	\
		(((u64)(x) & 0x000000ff00000000ull) >>  8) |	\
		(((u64)(x) & 0x00000000ff000000ull) <<  8) |	\
		(((u64)(x) & 0x0000000000ff0000ull) << 24) |	\
		(((u64)(x) & 0x000000000000ff00ull) << 40) |	\
		(((u64)(x) & 0x00000000000000ffull) << 56))

#ifdef HAVE_BYTESWAP_H
#	include <byteswap.h>
#else
#	define bswap_16(x) __ntfs_bswap_constant_16(x)
#	define bswap_32(x) __ntfs_bswap_constant_32(x)
#	define bswap_64(x) __ntfs_bswap_constant_64(x)
#endif

#if defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)

#define __le16_to_cpu(x) (x)
#define __le32_to_cpu(x) (x)
#define __le64_to_cpu(x) (x)

#define __cpu_to_le16(x) (x)
#define __cpu_to_le32(x) (x)
#define __cpu_to_le64(x) (x)

#define __constant_le16_to_cpu(x) (x)
#define __constant_le32_to_cpu(x) (x)
#define __constant_le64_to_cpu(x) (x)

#define __constant_cpu_to_le16(x) (x)
#define __constant_cpu_to_le32(x) (x)
#define __constant_cpu_to_le64(x) (x)

#elif defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)

#define __le16_to_cpu(x) bswap_16(x)
#define __le32_to_cpu(x) bswap_32(x)
#define __le64_to_cpu(x) bswap_64(x)

#define __cpu_to_le16(x) bswap_16(x)
#define __cpu_to_le32(x) bswap_32(x)
#define __cpu_to_le64(x) bswap_64(x)

#define __constant_le16_to_cpu(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_le32_to_cpu(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_le64_to_cpu(x) __ntfs_bswap_constant_64((u64)(x))

#define __constant_cpu_to_le16(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_cpu_to_le32(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_cpu_to_le64(x) __ntfs_bswap_constant_64((u64)(x))

#else

#error "You must define __BYTE_ORDER to be __LITTLE_ENDIAN or __BIG_ENDIAN."

#endif

/* Unsigned from LE to CPU conversion. */

#define le16_to_cpu(x)		(u16)__le16_to_cpu((u16)(x))
#define le32_to_cpu(x)		(u32)__le32_to_cpu((u32)(x))
#define le64_to_cpu(x)		(u64)__le64_to_cpu((u64)(x))

#define le16_to_cpup(x)		(u16)__le16_to_cpu(*(const u16*)(x))
#define le32_to_cpup(x)		(u32)__le32_to_cpu(*(const u32*)(x))
#define le64_to_cpup(x)		(u64)__le64_to_cpu(*(const u64*)(x))

/* Signed from LE to CPU conversion. */

#define sle16_to_cpu(x)		(s16)__le16_to_cpu((s16)(x))
#define sle32_to_cpu(x)		(s32)__le32_to_cpu((s32)(x))
#define sle64_to_cpu(x)		(s64)__le64_to_cpu((s64)(x))

#define sle16_to_cpup(x)	(s16)__le16_to_cpu(*(s16*)(x))
#define sle32_to_cpup(x)	(s32)__le32_to_cpu(*(s32*)(x))
#define sle64_to_cpup(x)	(s64)__le64_to_cpu(*(s64*)(x))

/* Unsigned from CPU to LE conversion. */

#define cpu_to_le16(x)		(u16)__cpu_to_le16((u16)(x))
#define cpu_to_le32(x)		(u32)__cpu_to_le32((u32)(x))
#define cpu_to_le64(x)		(u64)__cpu_to_le64((u64)(x))

#define cpu_to_le16p(x)		(u16)__cpu_to_le16(*(u16*)(x))
#define cpu_to_le32p(x)		(u32)__cpu_to_le32(*(u32*)(x))
#define cpu_to_le64p(x)		(u64)__cpu_to_le64(*(u64*)(x))

/* Signed from CPU to LE conversion. */

#define cpu_to_sle16(x)		(s16)__cpu_to_le16((s16)(x))
#define cpu_to_sle32(x)		(s32)__cpu_to_le32((s32)(x))
#define cpu_to_sle64(x)		(s64)__cpu_to_le64((s64)(x))

#define cpu_to_sle16p(x)	(s16)__cpu_to_le16(*(s16*)(x))
#define cpu_to_sle32p(x)	(s32)__cpu_to_le32(*(s32*)(x))
#define cpu_to_sle64p(x)	(s64)__cpu_to_le64(*(s64*)(x))

/* Constant endianness conversion defines. */

#define const_le16_to_cpu(x)	__constant_le16_to_cpu(x)
#define const_le32_to_cpu(x)	__constant_le32_to_cpu(x)
#define const_le64_to_cpu(x)	__constant_le64_to_cpu(x)

#define const_cpu_to_le16(x)	__constant_cpu_to_le16(x)
#define const_cpu_to_le32(x)	__constant_cpu_to_le32(x)
#define const_cpu_to_le64(x)	__constant_cpu_to_le64(x)

#endif /* defined _NTFS_ENDIANS_H */
