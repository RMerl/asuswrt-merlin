#ifndef _MISSING_H_
#define _MISSING_H_

#include <endian.h>
#include <byteswap.h>
#include <errno.h>
#include <stdint.h>

#define MAXBSIZE		(256 * 4096)

#ifndef true
#define true			1
#endif
#ifndef false
#define false			0
#endif

/* Mac types */

/* 8 Bit */
#ifndef UInt8
#define UInt8			uint8_t
#endif
#ifndef u_int8_t
#define u_int8_t		UInt8
#endif
#ifndef SInt8
#define SInt8			int8_t
#endif

/* 16 Bit */
#ifndef UInt16
#define UInt16			uint16_t
#endif
#ifndef u_int16_t
#define u_int16_t		UInt16
#endif
#ifndef SInt16
#define SInt16			int16_t
#endif

/* 32 Bit */
#ifndef UInt32
#define UInt32			uint32_t
#endif
#ifndef u_int32_t
#define u_int32_t		UInt32
#endif
#ifndef SInt32
#define SInt32			int32_t
#endif

/* 64 Bit */
#ifndef UInt64
#define UInt64			uint64_t
#endif
#ifndef u_int64_t
#define u_int64_t		UInt64
#endif
#ifndef SInt64
#define SInt64			int64_t
#endif

#define UniChar			u_int16_t
#define Boolean			u_int8_t

#define UF_NODUMP	0x00000001

/* syslimits.h */
#define NAME_MAX	255

/* Byteswap stuff */
#define NXSwapHostLongToBig(x)		cpu_to_be64(x)
#define NXSwapBigShortToHost(x) 	be16_to_cpu(x)
#define OSSwapBigToHostInt16(x)		be16_to_cpu(x)
#define NXSwapBigLongToHost(x)		be32_to_cpu(x)
#define OSSwapBigToHostInt32(x)		be32_to_cpu(x)
#define NXSwapBigLongLongToHost(x) 	be64_to_cpu(x)
#define OSSwapBigToHostInt64(x)		be64_to_cpu(x)

#if __BYTE_ORDER == __LITTLE_ENDIAN
/* Big Endian Swaps */
#ifndef be16_to_cpu
#define be16_to_cpu(x) bswap_16(x)
#endif
#ifndef be32_to_cpu
#define be32_to_cpu(x) bswap_32(x)
#endif
#ifndef be64_to_cpu
#define be64_to_cpu(x) bswap_64(x)
#endif
#ifndef cpu_to_be64
#define cpu_to_be64(x) bswap_64(x) 
#endif
#elif __BYTE_ORDER == __BIG_ENDIAN
/* Big endian doesn't swap */
#ifndef be16_to_cpu
#define be16_to_cpu(x)	(x)
#endif
#ifndef be32_to_cpu
#define be32_to_cpu(x)	(x)
#endif
#ifndef be64_to_cpu
#define be64_to_cpu(x)	(x)
#endif
#ifndef cpu_to_be64
#define cpu_to_be64(x) 	(x)
#endif
#endif

#define KAUTH_FILESEC_XATTR "com.apple.system.Security"

#endif
