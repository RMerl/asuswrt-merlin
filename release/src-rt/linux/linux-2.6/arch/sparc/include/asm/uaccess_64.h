#ifndef _ASM_UACCESS_H
#define _ASM_UACCESS_H

/*
 * User space memory access functions
 */

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/thread_info.h>
#include <asm/asi.h>
#include <asm/system.h>
#include <asm/spitfire.h>
#include <asm-generic/uaccess-unaligned.h>
#endif

#ifndef __ASSEMBLY__

/*
 * Sparc64 is segmented, though more like the M68K than the I386.
 * We use the secondary ASI to address user memory, which references a
 * completely different VM map, thus there is zero chance of the user
 * doing something queer and tricking us into poking kernel memory.
 *
 * What is left here is basically what is needed for the other parts of
 * the kernel that expect to be able to manipulate, erum, "segments".
 * Or perhaps more properly, permissions.
 *
 * "For historical reasons, these macros are grossly misnamed." -Linus
 */

#define KERNEL_DS   ((mm_segment_t) { ASI_P })
#define USER_DS     ((mm_segment_t) { ASI_AIUS })	/* har har har */

#define VERIFY_READ	0
#define VERIFY_WRITE	1

#define get_fs() ((mm_segment_t) { get_thread_current_ds() })
#define get_ds() (KERNEL_DS)

#define segment_eq(a,b)  ((a).seg == (b).seg)

#define set_fs(val)								\
do {										\
	set_thread_current_ds((val).seg);					\
	__asm__ __volatile__ ("wr %%g0, %0, %%asi" : : "r" ((val).seg));	\
} while(0)

static inline int __access_ok(const void __user * addr, unsigned long size)
{
	return 1;
}

static inline int access_ok(int type, const void __user * addr, unsigned long size)
{
	return 1;
}

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
        unsigned int insn, fixup;
};

extern void __ret_efault(void);
extern void __retl_efault(void);

/* Uh, these should become the main single-value transfer routines..
 * They automatically use the right size if we just have the right
 * pointer type..
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the ugliness from the user.
 */
#define put_user(x,ptr) ({ \
unsigned long __pu_addr = (unsigned long)(ptr); \
__chk_user_ptr(ptr); \
__put_user_nocheck((__typeof__(*(ptr)))(x),__pu_addr,sizeof(*(ptr))); })

#define get_user(x,ptr) ({ \
unsigned long __gu_addr = (unsigned long)(ptr); \
__chk_user_ptr(ptr); \
__get_user_nocheck((x),__gu_addr,sizeof(*(ptr)),__typeof__(*(ptr))); })

#define __put_user(x,ptr) put_user(x,ptr)
#define __get_user(x,ptr) get_user(x,ptr)

struct __large_struct { unsigned long buf[100]; };
#define __m(x) ((struct __large_struct *)(x))

#define __put_user_nocheck(data,addr,size) ({ \
register int __pu_ret; \
switch (size) { \
case 1: __put_user_asm(data,b,addr,__pu_ret); break; \
case 2: __put_user_asm(data,h,addr,__pu_ret); break; \
case 4: __put_user_asm(data,w,addr,__pu_ret); break; \
case 8: __put_user_asm(data,x,addr,__pu_ret); break; \
default: __pu_ret = __put_user_bad(); break; \
} __pu_ret; })

#define __put_user_asm(x,size,addr,ret)					\
__asm__ __volatile__(							\
	"/* Put user asm, inline. */\n"					\
"1:\t"	"st"#size "a %1, [%2] %%asi\n\t"				\
	"clr	%0\n"							\
"2:\n\n\t"								\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"sethi	%%hi(2b), %0\n\t"					\
	"jmpl	%0 + %%lo(2b), %%g0\n\t"				\
	" mov	%3, %0\n\n\t"						\
	".previous\n\t"							\
	".section __ex_table,\"a\"\n\t"					\
	".align	4\n\t"							\
	".word	1b, 3b\n\t"						\
	".previous\n\n\t"						\
       : "=r" (ret) : "r" (x), "r" (__m(addr)),				\
	 "i" (-EFAULT))

extern int __put_user_bad(void);

#define __get_user_nocheck(data,addr,size,type) ({ \
register int __gu_ret; \
register unsigned long __gu_val; \
switch (size) { \
case 1: __get_user_asm(__gu_val,ub,addr,__gu_ret); break; \
case 2: __get_user_asm(__gu_val,uh,addr,__gu_ret); break; \
case 4: __get_user_asm(__gu_val,uw,addr,__gu_ret); break; \
case 8: __get_user_asm(__gu_val,x,addr,__gu_ret); break; \
default: __gu_val = 0; __gu_ret = __get_user_bad(); break; \
} data = (type) __gu_val; __gu_ret; })

#define __get_user_nocheck_ret(data,addr,size,type,retval) ({ \
register unsigned long __gu_val __asm__ ("l1"); \
switch (size) { \
case 1: __get_user_asm_ret(__gu_val,ub,addr,retval); break; \
case 2: __get_user_asm_ret(__gu_val,uh,addr,retval); break; \
case 4: __get_user_asm_ret(__gu_val,uw,addr,retval); break; \
case 8: __get_user_asm_ret(__gu_val,x,addr,retval); break; \
default: if (__get_user_bad()) return retval; \
} data = (type) __gu_val; })

#define __get_user_asm(x,size,addr,ret)					\
__asm__ __volatile__(							\
	"/* Get user asm, inline. */\n"					\
"1:\t"	"ld"#size "a [%2] %%asi, %1\n\t"				\
	"clr	%0\n"							\
"2:\n\n\t"								\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"sethi	%%hi(2b), %0\n\t"					\
	"clr	%1\n\t"							\
	"jmpl	%0 + %%lo(2b), %%g0\n\t"				\
	" mov	%3, %0\n\n\t"						\
	".previous\n\t"							\
	".section __ex_table,\"a\"\n\t"					\
	".align	4\n\t"							\
	".word	1b, 3b\n\n\t"						\
	".previous\n\t"							\
       : "=r" (ret), "=r" (x) : "r" (__m(addr)),			\
	 "i" (-EFAULT))

#define __get_user_asm_ret(x,size,addr,retval)				\
if (__builtin_constant_p(retval) && retval == -EFAULT)			\
__asm__ __volatile__(							\
	"/* Get user asm ret, inline. */\n"				\
"1:\t"	"ld"#size "a [%1] %%asi, %0\n\n\t"				\
	".section __ex_table,\"a\"\n\t"					\
	".align	4\n\t"							\
	".word	1b,__ret_efault\n\n\t"					\
	".previous\n\t"							\
       : "=r" (x) : "r" (__m(addr)));					\
else									\
__asm__ __volatile__(							\
	"/* Get user asm ret, inline. */\n"				\
"1:\t"	"ld"#size "a [%1] %%asi, %0\n\n\t"				\
	".section .fixup,#alloc,#execinstr\n\t"				\
	".align	4\n"							\
"3:\n\t"								\
	"ret\n\t"							\
	" restore %%g0, %2, %%o0\n\n\t"					\
	".previous\n\t"							\
	".section __ex_table,\"a\"\n\t"					\
	".align	4\n\t"							\
	".word	1b, 3b\n\n\t"						\
	".previous\n\t"							\
       : "=r" (x) : "r" (__m(addr)), "i" (retval))

extern int __get_user_bad(void);

extern unsigned long __must_check ___copy_from_user(void *to,
						    const void __user *from,
						    unsigned long size);
extern unsigned long copy_from_user_fixup(void *to, const void __user *from,
					  unsigned long size);
static inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long size)
{
	unsigned long ret = ___copy_from_user(to, from, size);

	if (unlikely(ret))
		ret = copy_from_user_fixup(to, from, size);

	return ret;
}
#define __copy_from_user copy_from_user

extern unsigned long __must_check ___copy_to_user(void __user *to,
						  const void *from,
						  unsigned long size);
extern unsigned long copy_to_user_fixup(void __user *to, const void *from,
					unsigned long size);
static inline unsigned long __must_check
copy_to_user(void __user *to, const void *from, unsigned long size)
{
	unsigned long ret = ___copy_to_user(to, from, size);

	if (unlikely(ret))
		ret = copy_to_user_fixup(to, from, size);
	return ret;
}
#define __copy_to_user copy_to_user

extern unsigned long __must_check ___copy_in_user(void __user *to,
						  const void __user *from,
						  unsigned long size);
extern unsigned long copy_in_user_fixup(void __user *to, void __user *from,
					unsigned long size);
static inline unsigned long __must_check
copy_in_user(void __user *to, void __user *from, unsigned long size)
{
	unsigned long ret = ___copy_in_user(to, from, size);

	if (unlikely(ret))
		ret = copy_in_user_fixup(to, from, size);
	return ret;
}
#define __copy_in_user copy_in_user

extern unsigned long __must_check __clear_user(void __user *, unsigned long);

#define clear_user __clear_user

extern long __must_check __strncpy_from_user(char *dest, const char __user *src, long count);

#define strncpy_from_user __strncpy_from_user

extern long __strlen_user(const char __user *);
extern long __strnlen_user(const char __user *, long len);

#define strlen_user __strlen_user
#define strnlen_user __strnlen_user
#define __copy_to_user_inatomic ___copy_to_user
#define __copy_from_user_inatomic ___copy_from_user

#endif  /* __ASSEMBLY__ */

#endif /* _ASM_UACCESS_H */
