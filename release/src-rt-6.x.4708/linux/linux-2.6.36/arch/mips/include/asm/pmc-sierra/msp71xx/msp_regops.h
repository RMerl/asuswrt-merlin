

#ifndef __ASM_REGOPS_H__
#define __ASM_REGOPS_H__

#include <linux/types.h>

#include <asm/war.h>

#ifndef R10000_LLSC_WAR
#define R10000_LLSC_WAR 0
#endif

#if R10000_LLSC_WAR == 1
#define __beqz	"beqzl	"
#else
#define __beqz	"beqz	"
#endif

#ifndef _LINUX_TYPES_H
typedef unsigned int u32;
#endif

/*
 * Sets all the masked bits to the corresponding value bits
 */
static inline void set_value_reg32(volatile u32 *const addr,
					u32 const mask,
					u32 const value)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	mips3				\n"
	"1:	ll	%0, %1	# set_value_reg32	\n"
	"	and	%0, %2				\n"
	"	or	%0, %3				\n"
	"	sc	%0, %1				\n"
	"	"__beqz"%0, 1b				\n"
	"	nop					\n"
	"	.set	pop				\n"
	: "=&r" (temp), "=m" (*addr)
	: "ir" (~mask), "ir" (value), "m" (*addr));
}

/*
 * Sets all the masked bits to '1'
 */
static inline void set_reg32(volatile u32 *const addr,
				u32 const mask)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	mips3				\n"
	"1:	ll	%0, %1		# set_reg32	\n"
	"	or	%0, %2				\n"
	"	sc	%0, %1				\n"
	"	"__beqz"%0, 1b				\n"
	"	nop					\n"
	"	.set	pop				\n"
	: "=&r" (temp), "=m" (*addr)
	: "ir" (mask), "m" (*addr));
}

/*
 * Sets all the masked bits to '0'
 */
static inline void clear_reg32(volatile u32 *const addr,
				u32 const mask)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	mips3				\n"
	"1:	ll	%0, %1		# clear_reg32	\n"
	"	and	%0, %2				\n"
	"	sc	%0, %1				\n"
	"	"__beqz"%0, 1b				\n"
	"	nop					\n"
	"	.set	pop				\n"
	: "=&r" (temp), "=m" (*addr)
	: "ir" (~mask), "m" (*addr));
}

/*
 * Toggles all masked bits from '0' to '1' and '1' to '0'
 */
static inline void toggle_reg32(volatile u32 *const addr,
				u32 const mask)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	mips3				\n"
	"1:	ll	%0, %1		# toggle_reg32	\n"
	"	xor	%0, %2				\n"
	"	sc	%0, %1				\n"
	"	"__beqz"%0, 1b				\n"
	"	nop					\n"
	"	.set	pop				\n"
	: "=&r" (temp), "=m" (*addr)
	: "ir" (mask), "m" (*addr));
}

/*
 * Read all masked bits others are returned as '0'
 */
static inline u32 read_reg32(volatile u32 *const addr,
				u32 const mask)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	noreorder			\n"
	"	lw	%0, %1		# read		\n"
	"	and	%0, %2		# mask		\n"
	"	.set	pop				\n"
	: "=&r" (temp)
	: "m" (*addr), "ir" (mask));

	return temp;
}

/*
 * blocking_read_reg32 - Read address with blocking load
 *
 * Uncached writes need to be read back to ensure they reach RAM.
 * The returned value must be 'used' to prevent from becoming a
 * non-blocking load.
 */
static inline u32 blocking_read_reg32(volatile u32 *const addr)
{
	u32 temp;

	__asm__ __volatile__(
	"	.set	push				\n"
	"	.set	noreorder			\n"
	"	lw	%0, %1		# read		\n"
	"	move	%0, %0		# block		\n"
	"	.set	pop				\n"
	: "=&r" (temp)
	: "m" (*addr));

	return temp;
}

/*
 * For special strange cases only:
 *
 * If you need custom processing within a ll/sc loop, use the following macros
 * VERY CAREFULLY:
 *
 *   u32 tmp;				<-- Define a variable to hold the data
 *
 *   custom_read_reg32(address, tmp);	<-- Reads the address and put the value
 *						in the 'tmp' variable given
 *
 *	From here on out, you are (basicly) atomic, so don't do anything too
 *	fancy!
 *	Also, this code may loop if the end of this block fails to write
 *	everything back safely due do the other CPU, so do NOT do anything
 *	with side-effects!
 *
 *   custom_write_reg32(address, tmp);	<-- Writes back 'tmp' safely.
 */
#define custom_read_reg32(address, tmp)				\
	__asm__ __volatile__(					\
	"	.set	push				\n"	\
	"	.set	mips3				\n"	\
	"1:	ll	%0, %1	#custom_read_reg32	\n"	\
	"	.set	pop				\n"	\
	: "=r" (tmp), "=m" (*address)				\
	: "m" (*address))

#define custom_write_reg32(address, tmp)			\
	__asm__ __volatile__(					\
	"	.set	push				\n"	\
	"	.set	mips3				\n"	\
	"	sc	%0, %1	#custom_write_reg32	\n"	\
	"	"__beqz"%0, 1b				\n"	\
	"	nop					\n"	\
	"	.set	pop				\n"	\
	: "=&r" (tmp), "=m" (*address)				\
	: "0" (tmp), "m" (*address))

#endif  /* __ASM_REGOPS_H__ */
