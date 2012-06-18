#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H

/* Somewhere in the middle of the GCC 2.96 development cycle, we implemented
   a mechanism by which the user can annotate likely branch directions and
   expect the blocks to be reordered appropriately.  Define __builtin_expect
   to nothing for earlier compilers.  */

#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif

#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)

#ifdef __attribute_used__
 #undef __attribute_used__
#endif

#if __GNUC__ > 3
#define __attribute_used__	__attribute__((__used__))
#elif __GNUC__ == 3
#if  __GNUC_MINOR__ >= 3
# define __attribute_used__	__attribute__((__used__))
#else
# define __attribute_used__	__attribute__((__unused__))
#endif /* __GNUC_MINOR__ >= 3 */
#elif __GNUC__ == 2
#define __attribute_used__	__attribute__((__unused__))
#else
#define __attribute_used__	/* not implemented */
#endif /* __GNUC__ */

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define __attribute_const__	__attribute__((__const__))
#else
#define __attribute_const__	/* unimplemented */
#endif

#if __GNUC__ >= 4 || __GNUC__ == 3 && __GNUC_MINOR__ >= 1
# define inline         __inline__ __attribute__((always_inline))
# define __inline__     __inline__ __attribute__((always_inline))
# define __inline       __inline__ __attribute__((always_inline))
#endif

/*
 * A trick to suppress uninitialized variable warning without generating any
 * code
 */
#define uninitialized_var(x) x = x


#if __GNUC__ >= 4
# define __compiler_offsetof(a,b) __builtin_offsetof(a,b)
#endif

#ifdef __KERNEL__
#if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 2
#error "GCC >= 4.2 miscompiles kernel 2.4, do not use it!"
#error "While the resulting kernel may boot, you will encounter random bugs"
#error "at runtime. Only versions 2.95.3 to 4.1 are known to work reliably."
#error "To build with another version, for instance 3.3, please do"
#error "   make bzImage CC=gcc-3.3 "
#endif
#endif

/* no checker support, so we unconditionally define this as (null) */
#define __user
#define __iomem

#endif /* __LINUX_COMPILER_H */
