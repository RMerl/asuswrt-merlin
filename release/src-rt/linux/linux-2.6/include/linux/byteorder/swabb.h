#ifndef _LINUX_BYTEORDER_SWABB_H
#define _LINUX_BYTEORDER_SWABB_H

/*
 * linux/byteorder/swabb.h
 * SWAp Bytes Bizarrely
 *	swaHHXX[ps]?(foo)
 *
 * Support for obNUXIous pdp-endian and other bizarre architectures.
 * Will Linux ever run on such ancient beasts? if not, this file
 * will be but a programming pearl. Still, it's a reminder that we
 * shouldn't be making too many assumptions when trying to be portable.
 *
 */

/*
 * Meaning of the names I chose (vaxlinux people feel free to correct them):
 * swahw32	swap 16-bit half-words in a 32-bit word
 * swahb32	swap 8-bit halves of each 16-bit half-word in a 32-bit word
 *
 * No 64-bit support yet. I don't know NUXI conventions for long longs.
 * I guarantee it will be a mess when it's there, though :->
 * It will be even worse if there are conflicting 64-bit conventions.
 * Hopefully, no one ever used 64-bit objects on NUXI machines.
 *
 */

#include <linux/types.h>

#define ___swahw32(x) \
({ \
	__u32 __x = (x); \
	((__u32)( \
		(((__u32)(__x) & (__u32)0x0000ffffUL) << 16) | \
		(((__u32)(__x) & (__u32)0xffff0000UL) >> 16) )); \
})
#define ___swahb32(x) \
({ \
	__u32 __x = (x); \
	((__u32)( \
		(((__u32)(__x) & (__u32)0x00ff00ffUL) << 8) | \
		(((__u32)(__x) & (__u32)0xff00ff00UL) >> 8) )); \
})

#define ___constant_swahw32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x0000ffffUL) << 16) | \
		(((__u32)(x) & (__u32)0xffff0000UL) >> 16) ))
#define ___constant_swahb32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x00ff00ffUL) << 8) | \
		(((__u32)(x) & (__u32)0xff00ff00UL) >> 8) ))

/*
 * provide defaults when no architecture-specific optimization is detected
 */
#ifndef __arch__swahw32
#  define __arch__swahw32(x) ___swahw32(x)
#endif
#ifndef __arch__swahb32
#  define __arch__swahb32(x) ___swahb32(x)
#endif

#ifndef __arch__swahw32p
#  define __arch__swahw32p(x) __swahw32(*(x))
#endif
#ifndef __arch__swahb32p
#  define __arch__swahb32p(x) __swahb32(*(x))
#endif

#ifndef __arch__swahw32s
#  define __arch__swahw32s(x) do { *(x) = __swahw32p((x)); } while (0)
#endif
#ifndef __arch__swahb32s
#  define __arch__swahb32s(x) do { *(x) = __swahb32p((x)); } while (0)
#endif


/*
 * Allow constant folding
 */
#define __swahw32(x) \
(__builtin_constant_p((__u32)(x)) ? \
 ___swahw32((x)) : \
 __fswahw32((x)))
#define __swahb32(x) \
(__builtin_constant_p((__u32)(x)) ? \
 ___swahb32((x)) : \
 __fswahb32((x)))


static inline __u32 __fswahw32(__u32 x)
{
	return __arch__swahw32(x);
}

static inline __u32 __swahw32p(__u32 *x)
{
	return __arch__swahw32p(x);
}

static inline void __swahw32s(__u32 *addr)
{
	__arch__swahw32s(addr);
}

static inline __u32 __fswahb32(__u32 x)
{
	return __arch__swahb32(x);
}

static inline __u32 __swahb32p(__u32 *x)
{
	return __arch__swahb32p(x);
}

static inline void __swahb32s(__u32 *addr)
{
	__arch__swahb32s(addr);
}

#ifdef __BYTEORDER_HAS_U64__
/*
 * Not supported yet
 */
#endif /* __BYTEORDER_HAS_U64__ */

#define swahw32 __swahw32
#define swahb32 __swahb32
#define swahw32p __swahw32p
#define swahb32p __swahb32p
#define swahw32s __swahw32s
#define swahb32s __swahb32s

#endif /* _LINUX_BYTEORDER_SWABB_H */
