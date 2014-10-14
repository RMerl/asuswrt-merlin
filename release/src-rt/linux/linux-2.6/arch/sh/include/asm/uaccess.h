#ifndef __ASM_SH_UACCESS_H
#define __ASM_SH_UACCESS_H

#include <linux/errno.h>
#include <linux/sched.h>
#include <asm/segment.h>

#define VERIFY_READ    0
#define VERIFY_WRITE   1

#define __addr_ok(addr) \
	((unsigned long __force)(addr) < current_thread_info()->addr_limit.seg)

/*
 * __access_ok: Check if address with size is OK or not.
 *
 * Uhhuh, this needs 33-bit arithmetic. We have a carry..
 *
 * sum := addr + size;  carry? --> flag = true;
 * if (sum >= addr_limit) flag = true;
 */
#define __access_ok(addr, size)		\
	(__addr_ok((addr) + (size)))
#define access_ok(type, addr, size)	\
	(__chk_user_ptr(addr),		\
	 __access_ok((unsigned long __force)(addr), (size)))

/*
 * Uh, these should become the main single-value transfer routines ...
 * They automatically use the right size if we just have the right
 * pointer type ...
 *
 * As SuperH uses the same address space for kernel and user data, we
 * can just do these as direct assignments.
 *
 * Careful to not
 * (a) re-use the arguments for side effects (sizeof is ok)
 * (b) require any knowledge of processes at this stage
 */
#define put_user(x,ptr)		__put_user_check((x), (ptr), sizeof(*(ptr)))
#define get_user(x,ptr)		__get_user_check((x), (ptr), sizeof(*(ptr)))

/*
 * The "__xxx" versions do not do address space checking, useful when
 * doing multiple accesses to the same area (the user has to do the
 * checks by hand with "access_ok()")
 */
#define __put_user(x,ptr)	__put_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __get_user(x,ptr)	__get_user_nocheck((x), (ptr), sizeof(*(ptr)))

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct __user *)(x))

#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err;						\
	unsigned long __gu_val;					\
	const __typeof__(*(ptr)) __user *__gu_addr = (ptr);	\
	__chk_user_ptr(ptr);					\
	__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

#define __get_user_check(x,ptr,size)					\
({									\
	long __gu_err = -EFAULT;					\
	unsigned long __gu_val = 0;					\
	const __typeof__(*(ptr)) *__gu_addr = (ptr);			\
	if (likely(access_ok(VERIFY_READ, __gu_addr, (size))))		\
		__get_user_size(__gu_val, __gu_addr, (size), __gu_err);	\
	(x) = (__typeof__(*(ptr)))__gu_val;				\
	__gu_err;							\
})

#define __put_user_nocheck(x,ptr,size)				\
({								\
	long __pu_err;						\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);		\
	__typeof__(*(ptr)) __pu_val = x;			\
	__chk_user_ptr(ptr);					\
	__put_user_size(__pu_val, __pu_addr, (size), __pu_err);	\
	__pu_err;						\
})

#define __put_user_check(x,ptr,size)				\
({								\
	long __pu_err = -EFAULT;				\
	__typeof__(*(ptr)) __user *__pu_addr = (ptr);		\
	__typeof__(*(ptr)) __pu_val = x;			\
	if (likely(access_ok(VERIFY_WRITE, __pu_addr, size)))	\
		__put_user_size(__pu_val, __pu_addr, (size),	\
				__pu_err);			\
	__pu_err;						\
})

#ifdef CONFIG_SUPERH32
# include "uaccess_32.h"
#else
# include "uaccess_64.h"
#endif

/* Generic arbitrary sized copy.  */
/* Return the number of bytes NOT copied */
__kernel_size_t __copy_user(void *to, const void *from, __kernel_size_t n);

static __always_inline unsigned long
__copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return __copy_user(to, (__force void *)from, n);
}

static __always_inline unsigned long __must_check
__copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return __copy_user((__force void *)to, from, n);
}

#define __copy_to_user_inatomic __copy_to_user
#define __copy_from_user_inatomic __copy_from_user

/*
 * Clear the area and return remaining number of bytes
 * (on failure.  Usually it's 0.)
 */
__kernel_size_t __clear_user(void *addr, __kernel_size_t size);

#define clear_user(addr,n)						\
({									\
	void __user * __cl_addr = (addr);				\
	unsigned long __cl_size = (n);					\
									\
	if (__cl_size && access_ok(VERIFY_WRITE,			\
		((unsigned long)(__cl_addr)), __cl_size))		\
		__cl_size = __clear_user(__cl_addr, __cl_size);		\
									\
	__cl_size;							\
})

/**
 * strncpy_from_user: - Copy a NUL terminated string from userspace.
 * @dst:   Destination address, in kernel space.  This buffer must be at
 *         least @count bytes long.
 * @src:   Source address, in user space.
 * @count: Maximum number of bytes to copy, including the trailing NUL.
 *
 * Copies a NUL-terminated string from userspace to kernel space.
 *
 * On success, returns the length of the string (not including the trailing
 * NUL).
 *
 * If access to userspace fails, returns -EFAULT (some data may have been
 * copied).
 *
 * If @count is smaller than the length of the string, copies @count bytes
 * and returns @count.
 */
#define strncpy_from_user(dest,src,count)				\
({									\
	unsigned long __sfu_src = (unsigned long)(src);			\
	int __sfu_count = (int)(count);					\
	long __sfu_res = -EFAULT;					\
									\
	if (__access_ok(__sfu_src, __sfu_count))			\
		__sfu_res = __strncpy_from_user((unsigned long)(dest),	\
				__sfu_src, __sfu_count);		\
									\
	__sfu_res;							\
})

static inline unsigned long
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long __copy_from = (unsigned long) from;
	__kernel_size_t __copy_size = (__kernel_size_t) n;

	if (__copy_size && __access_ok(__copy_from, __copy_size))
		return __copy_user(to, from, __copy_size);

	return __copy_size;
}

static inline unsigned long
copy_to_user(void __user *to, const void *from, unsigned long n)
{
	unsigned long __copy_to = (unsigned long) to;
	__kernel_size_t __copy_size = (__kernel_size_t) n;

	if (__copy_size && __access_ok(__copy_to, __copy_size))
		return __copy_user(to, from, __copy_size);

	return __copy_size;
}

/**
 * strnlen_user: - Get the size of a string in user space.
 * @s: The string to measure.
 * @n: The maximum valid length
 *
 * Context: User context only.  This function may sleep.
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 * If the string is too long, returns a value greater than @n.
 */
static inline long strnlen_user(const char __user *s, long n)
{
	if (!__addr_ok(s))
		return 0;
	else
		return __strnlen_user(s, n);
}

/**
 * strlen_user: - Get the size of a string in user space.
 * @str: The string to measure.
 *
 * Context: User context only.  This function may sleep.
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 *
 * If there is a limit on the length of a valid string, you may wish to
 * consider using strnlen_user() instead.
 */
#define strlen_user(str)	strnlen_user(str, ~0UL >> 1)

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */
struct exception_table_entry {
	unsigned long insn, fixup;
};

#if defined(CONFIG_SUPERH64) && defined(CONFIG_MMU)
#define ARCH_HAS_SEARCH_EXTABLE
#endif

int fixup_exception(struct pt_regs *regs);
/* Returns 0 if exception not found and fixup.unit otherwise.  */
unsigned long search_exception_table(unsigned long addr);
const struct exception_table_entry *search_exception_tables(unsigned long addr);


#endif /* __ASM_SH_UACCESS_H */
