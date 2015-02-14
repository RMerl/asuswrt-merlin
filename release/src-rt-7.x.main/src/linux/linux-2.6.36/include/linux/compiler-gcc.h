/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
#ifndef __LINUX_COMPILER_H
#error "Please don't include <linux/compiler-gcc.h> directly, include <linux/compiler.h> instead."
#endif

/*
 * Common definitions for all gcc versions go here.
 */


/* Optimization barrier */
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("": : :"memory")

#define RELOC_HIDE(ptr, off)					\
  ({ unsigned long __ptr;					\
    __asm__ ("" : "=r"(__ptr) : "0"(ptr));		\
    (typeof(ptr)) (__ptr + (off)); })

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))

/*
 * Force always-inline if the user requests it so via the .config,
 * or if gcc is too old:
 */
#if !defined(CONFIG_ARCH_SUPPORTS_OPTIMIZED_INLINING) || \
	!defined(CONFIG_OPTIMIZE_INLINING) || (__GNUC__ < 4)
#if !defined(CONFIG_BUZZZ_FUNC)
# define inline		inline		__attribute__((always_inline))
# define __inline__	__inline__	__attribute__((always_inline))
# define __inline	__inline	__attribute__((always_inline))
#endif	/* !CONFIG_BUZZZ_FUNC */
#endif

#define __deprecated			__attribute__((deprecated))
#define __packed			__attribute__((packed))
#define __weak				__attribute__((weak))

#define __naked				__attribute__((naked)) noinline __noclone notrace

#define __noreturn			__attribute__((noreturn))

/*
 * From the GCC manual:
 *
 * Many functions have no effects except the return value and their
 * return value depends only on the parameters and/or global
 * variables.  Such a function can be subject to common subexpression
 * elimination and loop optimization just as an arithmetic operator
 * would be.
 * [...]
 */
#define __pure				__attribute__((pure))
#define __aligned(x)			__attribute__((aligned(x)))
#define __printf(a,b)			__attribute__((format(printf,a,b)))
#define  noinline			__attribute__((noinline))
#define __attribute_const__		__attribute__((__const__))
#define __maybe_unused			__attribute__((unused))
#define __always_unused			__attribute__((unused))

#define __gcc_header(x) #x
#define _gcc_header(x) __gcc_header(linux/compiler-gcc##x.h)
#define gcc_header(x) _gcc_header(x)
#include gcc_header(__GNUC__)

#if !defined(__noclone)
#define __noclone	/* not needed */
#endif
