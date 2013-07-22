/*
 * Greate deal of the code was taken from the kernel UBIFS implementation, and
 * this file contains some "glue" definitions.
 */

#ifndef __UBIFS_DEFS_H__
#define __UBIFS_DEFS_H__

#define t16(x) ({ \
	uint16_t __b = (x); \
	(__LITTLE_ENDIAN==__BYTE_ORDER) ? __b : bswap_16(__b); \
})

#define t32(x) ({ \
	uint32_t __b = (x); \
	(__LITTLE_ENDIAN==__BYTE_ORDER) ? __b : bswap_32(__b); \
})

#define t64(x) ({ \
	uint64_t __b = (x); \
	(__LITTLE_ENDIAN==__BYTE_ORDER) ? __b : bswap_64(__b); \
})

#define cpu_to_le16(x) ((__le16){t16(x)})
#define cpu_to_le32(x) ((__le32){t32(x)})
#define cpu_to_le64(x) ((__le64){t64(x)})

#define le16_to_cpu(x) (t16((x)))
#define le32_to_cpu(x) (t32((x)))
#define le64_to_cpu(x) (t64((x)))

#define ALIGN(x,a) __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask) (((x)+(mask))&~(mask))

#define min_t(t,x,y) ({ \
	typeof((x)) _x = (x); \
	typeof((y)) _y = (y); \
	(_x < _y) ? _x : _y; \
})

#define max_t(t,x,y) ({ \
	typeof((x)) _x = (x); \
	typeof((y)) _y = (y); \
	(_x > _y) ? _x : _y; \
})

#define unlikely(x) (x)

#define ubifs_assert(x) ({})

struct qstr
{
	char *name;
	size_t len;
};

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })

#if INT_MAX != 0x7fffffff
#error : sizeof(int) must be 4 for this program
#endif

#if (~0ULL) != 0xffffffffffffffffULL
#error : sizeof(long long) must be 8 for this program
#endif

#endif
