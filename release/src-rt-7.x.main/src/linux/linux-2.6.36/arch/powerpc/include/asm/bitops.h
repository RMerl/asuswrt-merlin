/*
 * PowerPC atomic bit operations.
 *
 * Merged version by David Gibson <david@gibson.dropbear.id.au>.
 * Based on ppc64 versions by: Dave Engebretsen, Todd Inglett, Don
 * Reed, Pat McCarthy, Peter Bergner, Anton Blanchard.  They
 * originally took it from the ppc32 code.
 *
 * Within a word, bits are numbered LSB first.  Lot's of places make
 * this assumption by directly testing bits with (val & (1<<nr)).
 * This can cause confusion for large (> 1 word) bitmaps on a
 * big-endian system because, unlike little endian, the number of each
 * bit depends on the word size.
 *
 * The bitop functions are defined to work on unsigned longs, so for a
 * ppc64 system the bits end up numbered:
 *   |63..............0|127............64|191...........128|255...........196|
 * and on ppc32:
 *   |31.....0|63....31|95....64|127...96|159..128|191..160|223..192|255..224|
 *
 * There are a few little-endian macros used mostly for filesystem
 * bitmaps, these work on similar bit arrays layouts, but
 * byte-oriented:
 *   |7...0|15...8|23...16|31...24|39...32|47...40|55...48|63...56|
 *
 * The main difference is that bit 3-5 (64b) or 3-4 (32b) in the bit
 * number field needs to be reversed compared to the big-endian bit
 * fields. This can be achieved by XOR with 0x38 (64b) or 0x18 (32b).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_POWERPC_BITOPS_H
#define _ASM_POWERPC_BITOPS_H

#ifdef __KERNEL__

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <asm/asm-compat.h>
#include <asm/synch.h>

/*
 * clear_bit doesn't imply a memory barrier
 */
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()

#define BITOP_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITOP_LE_SWIZZLE	((BITS_PER_LONG-1) & ~0x7)

/* Macro for generating the ***_bits() functions */
#define DEFINE_BITOP(fn, op, prefix, postfix)	\
static __inline__ void fn(unsigned long mask,	\
		volatile unsigned long *_p)	\
{						\
	unsigned long old;			\
	unsigned long *p = (unsigned long *)_p;	\
	__asm__ __volatile__ (			\
	prefix					\
"1:"	PPC_LLARX(%0,0,%3,0) "\n"		\
	stringify_in_c(op) "%0,%0,%2\n"		\
	PPC405_ERR77(0,%3)			\
	PPC_STLCX "%0,0,%3\n"			\
	"bne- 1b\n"				\
	postfix					\
	: "=&r" (old), "+m" (*p)		\
	: "r" (mask), "r" (p)			\
	: "cc", "memory");			\
}

DEFINE_BITOP(set_bits, or, "", "")
DEFINE_BITOP(clear_bits, andc, "", "")
DEFINE_BITOP(clear_bits_unlock, andc, PPC_RELEASE_BARRIER, "")
DEFINE_BITOP(change_bits, xor, "", "")

static __inline__ void set_bit(int nr, volatile unsigned long *addr)
{
	set_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr));
}

static __inline__ void clear_bit(int nr, volatile unsigned long *addr)
{
	clear_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr));
}

static __inline__ void clear_bit_unlock(int nr, volatile unsigned long *addr)
{
	clear_bits_unlock(BITOP_MASK(nr), addr + BITOP_WORD(nr));
}

static __inline__ void change_bit(int nr, volatile unsigned long *addr)
{
	change_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr));
}

/* Like DEFINE_BITOP(), with changes to the arguments to 'op' and the output
 * operands. */
#define DEFINE_TESTOP(fn, op, prefix, postfix, eh)	\
static __inline__ unsigned long fn(			\
		unsigned long mask,			\
		volatile unsigned long *_p)		\
{							\
	unsigned long old, t;				\
	unsigned long *p = (unsigned long *)_p;		\
	__asm__ __volatile__ (				\
	prefix						\
"1:"	PPC_LLARX(%0,0,%3,eh) "\n"			\
	stringify_in_c(op) "%1,%0,%2\n"			\
	PPC405_ERR77(0,%3)				\
	PPC_STLCX "%1,0,%3\n"				\
	"bne- 1b\n"					\
	postfix						\
	: "=&r" (old), "=&r" (t)			\
	: "r" (mask), "r" (p)				\
	: "cc", "memory");				\
	return (old & mask);				\
}

DEFINE_TESTOP(test_and_set_bits, or, PPC_RELEASE_BARRIER,
	      PPC_ACQUIRE_BARRIER, 0)
DEFINE_TESTOP(test_and_set_bits_lock, or, "",
	      PPC_ACQUIRE_BARRIER, 1)
DEFINE_TESTOP(test_and_clear_bits, andc, PPC_RELEASE_BARRIER,
	      PPC_ACQUIRE_BARRIER, 0)
DEFINE_TESTOP(test_and_change_bits, xor, PPC_RELEASE_BARRIER,
	      PPC_ACQUIRE_BARRIER, 0)

static __inline__ int test_and_set_bit(unsigned long nr,
				       volatile unsigned long *addr)
{
	return test_and_set_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr)) != 0;
}

static __inline__ int test_and_set_bit_lock(unsigned long nr,
				       volatile unsigned long *addr)
{
	return test_and_set_bits_lock(BITOP_MASK(nr),
				addr + BITOP_WORD(nr)) != 0;
}

static __inline__ int test_and_clear_bit(unsigned long nr,
					 volatile unsigned long *addr)
{
	return test_and_clear_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr)) != 0;
}

static __inline__ int test_and_change_bit(unsigned long nr,
					  volatile unsigned long *addr)
{
	return test_and_change_bits(BITOP_MASK(nr), addr + BITOP_WORD(nr)) != 0;
}

#include <asm-generic/bitops/non-atomic.h>

static __inline__ void __clear_bit_unlock(int nr, volatile unsigned long *addr)
{
	__asm__ __volatile__(PPC_RELEASE_BARRIER "" ::: "memory");
	__clear_bit(nr, addr);
}

/*
 * Return the zero-based bit position (LE, not IBM bit numbering) of
 * the most significant 1-bit in a double word.
 */
static __inline__ __attribute__((const))
int __ilog2(unsigned long x)
{
	int lz;

	asm (PPC_CNTLZL "%0,%1" : "=r" (lz) : "r" (x));
	return BITS_PER_LONG - 1 - lz;
}

static inline __attribute__((const))
int __ilog2_u32(u32 n)
{
	int bit;
	asm ("cntlzw %0,%1" : "=r" (bit) : "r" (n));
	return 31 - bit;
}

#ifdef __powerpc64__
static inline __attribute__((const))
int __ilog2_u64(u64 n)
{
	int bit;
	asm ("cntlzd %0,%1" : "=r" (bit) : "r" (n));
	return 63 - bit;
}
#endif

/*
 * Determines the bit position of the least significant 0 bit in the
 * specified double word. The returned bit position will be
 * zero-based, starting from the right side (63/31 - 0).
 */
static __inline__ unsigned long ffz(unsigned long x)
{
	/* no zero exists anywhere in the 8 byte area. */
	if ((x = ~x) == 0)
		return BITS_PER_LONG;

	/*
	 * Calculate the bit position of the least signficant '1' bit in x
	 * (since x has been changed this will actually be the least signficant
	 * '0' bit in * the original x).  Note: (x & -x) gives us a mask that
	 * is the least significant * (RIGHT-most) 1-bit of the value in x.
	 */
	return __ilog2(x & -x);
}

static __inline__ int __ffs(unsigned long x)
{
	return __ilog2(x & -x);
}

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */
static __inline__ int ffs(int x)
{
	unsigned long i = (unsigned long)x;
	return __ilog2(i & -i) + 1;
}

/*
 * fls: find last (most-significant) bit set.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static __inline__ int fls(unsigned int x)
{
	int lz;

	asm ("cntlzw %0,%1" : "=r" (lz) : "r" (x));
	return 32 - lz;
}

static __inline__ unsigned long __fls(unsigned long x)
{
	return __ilog2(x);
}

/*
 * 64-bit can do this using one cntlzd (count leading zeroes doubleword)
 * instruction; for 32-bit we use the generic version, which does two
 * 32-bit fls calls.
 */
#ifdef __powerpc64__
static __inline__ int fls64(__u64 x)
{
	int lz;

	asm ("cntlzd %0,%1" : "=r" (lz) : "r" (x));
	return 64 - lz;
}
#else
#include <asm-generic/bitops/fls64.h>
#endif /* __powerpc64__ */

#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/find.h>

/* Little-endian versions */

static __inline__ int test_le_bit(unsigned long nr,
				  __const__ unsigned long *addr)
{
	__const__ unsigned char	*tmp = (__const__ unsigned char *) addr;
	return (tmp[nr >> 3] >> (nr & 7)) & 1;
}

#define __set_le_bit(nr, addr) \
	__set_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))
#define __clear_le_bit(nr, addr) \
	__clear_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))

#define test_and_set_le_bit(nr, addr) \
	test_and_set_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))
#define test_and_clear_le_bit(nr, addr) \
	test_and_clear_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))

#define __test_and_set_le_bit(nr, addr) \
	__test_and_set_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))
#define __test_and_clear_le_bit(nr, addr) \
	__test_and_clear_bit((nr) ^ BITOP_LE_SWIZZLE, (addr))

#define find_first_zero_le_bit(addr, size) generic_find_next_zero_le_bit((addr), (size), 0)
unsigned long generic_find_next_zero_le_bit(const unsigned long *addr,
				    unsigned long size, unsigned long offset);

unsigned long generic_find_next_le_bit(const unsigned long *addr,
				    unsigned long size, unsigned long offset);
/* Bitmap functions for the ext2 filesystem */

#define ext2_set_bit(nr,addr) \
	__test_and_set_le_bit((nr), (unsigned long*)addr)
#define ext2_clear_bit(nr, addr) \
	__test_and_clear_le_bit((nr), (unsigned long*)addr)

#define ext2_set_bit_atomic(lock, nr, addr) \
	test_and_set_le_bit((nr), (unsigned long*)addr)
#define ext2_clear_bit_atomic(lock, nr, addr) \
	test_and_clear_le_bit((nr), (unsigned long*)addr)

#define ext2_test_bit(nr, addr)      test_le_bit((nr),(unsigned long*)addr)

#define ext2_find_first_zero_bit(addr, size) \
	find_first_zero_le_bit((unsigned long*)addr, size)
#define ext2_find_next_zero_bit(addr, size, off) \
	generic_find_next_zero_le_bit((unsigned long*)addr, size, off)

#define ext2_find_next_bit(addr, size, off) \
	generic_find_next_le_bit((unsigned long *)addr, size, off)
/* Bitmap functions for the minix filesystem.  */

#define minix_test_and_set_bit(nr,addr) \
	__test_and_set_le_bit(nr, (unsigned long *)addr)
#define minix_set_bit(nr,addr) \
	__set_le_bit(nr, (unsigned long *)addr)
#define minix_test_and_clear_bit(nr,addr) \
	__test_and_clear_le_bit(nr, (unsigned long *)addr)
#define minix_test_bit(nr,addr) \
	test_le_bit(nr, (unsigned long *)addr)

#define minix_find_first_zero_bit(addr,size) \
	find_first_zero_le_bit((unsigned long *)addr, size)

#include <asm-generic/bitops/sched.h>

#endif /* __KERNEL__ */

#endif /* _ASM_POWERPC_BITOPS_H */
