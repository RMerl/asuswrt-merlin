/*
 * Export of symbols defined in assembler
 */

/* Tell string.h we don't want memcpy etc. as cpp defines */
#define EXPORT_SYMTAB_STROPS

#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include <asm/checksum.h>
#include <asm/uaccess.h>
#include <asm/ftrace.h>

/* string functions */
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(__strlen_user);
EXPORT_SYMBOL(__strnlen_user);
EXPORT_SYMBOL(strncmp);

/* mem* functions */
extern void *__memscan_zero(void *, size_t);
extern void *__memscan_generic(void *, int, size_t);
extern void *__bzero(void *, size_t);

EXPORT_SYMBOL(memscan);
EXPORT_SYMBOL(__memscan_zero);
EXPORT_SYMBOL(__memscan_generic);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(__bzero);

/* Moving data to/from/in userspace. */
EXPORT_SYMBOL(__strncpy_from_user);

/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial);

#ifdef CONFIG_MCOUNT
EXPORT_SYMBOL(_mcount);
#endif

/*
 * sparc
 */
#ifdef CONFIG_SPARC32
extern int __ashrdi3(int, int);
extern int __ashldi3(int, int);
extern int __lshrdi3(int, int);
extern int __muldi3(int, int);
extern int __divdi3(int, int);

extern void (*__copy_1page)(void *, const void *);
extern void (*bzero_1page)(void *);

extern int __strncmp(const char *, const char *, __kernel_size_t);

extern void ___rw_read_enter(void);
extern void ___rw_read_try(void);
extern void ___rw_read_exit(void);
extern void ___rw_write_enter(void);
extern void ___atomic24_add(void);
extern void ___atomic24_sub(void);

/* Alias functions whose names begin with "." and export the aliases.
 * The module references will be fixed up by module_frob_arch_sections.
 */
extern int _Div(int, int);
extern int _Mul(int, int);
extern int _Rem(int, int);
extern unsigned _Udiv(unsigned, unsigned);
extern unsigned _Umul(unsigned, unsigned);
extern unsigned _Urem(unsigned, unsigned);

/* Networking helper routines. */
EXPORT_SYMBOL(__csum_partial_copy_sparc_generic);

/* Special internal versions of library functions. */
EXPORT_SYMBOL(__copy_1page);
EXPORT_SYMBOL(__memmove);
EXPORT_SYMBOL(bzero_1page);

/* string functions */
EXPORT_SYMBOL(__strncmp);

/* Moving data to/from/in userspace. */
EXPORT_SYMBOL(__copy_user);

/* Used by asm/spinlock.h */
#ifdef CONFIG_SMP
EXPORT_SYMBOL(___rw_read_enter);
EXPORT_SYMBOL(___rw_read_try);
EXPORT_SYMBOL(___rw_read_exit);
EXPORT_SYMBOL(___rw_write_enter);
#endif

/* Atomic operations. */
EXPORT_SYMBOL(___atomic24_add);
EXPORT_SYMBOL(___atomic24_sub);

EXPORT_SYMBOL(__ashrdi3);
EXPORT_SYMBOL(__ashldi3);
EXPORT_SYMBOL(__lshrdi3);
EXPORT_SYMBOL(__muldi3);
EXPORT_SYMBOL(__divdi3);

EXPORT_SYMBOL(_Rem);
EXPORT_SYMBOL(_Urem);
EXPORT_SYMBOL(_Mul);
EXPORT_SYMBOL(_Umul);
EXPORT_SYMBOL(_Div);
EXPORT_SYMBOL(_Udiv);
#endif

/*
 * sparc64
 */
#ifdef CONFIG_SPARC64
/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial_copy_nocheck);
EXPORT_SYMBOL(__csum_partial_copy_from_user);
EXPORT_SYMBOL(__csum_partial_copy_to_user);
EXPORT_SYMBOL(ip_fast_csum);

/* Moving data to/from/in userspace. */
EXPORT_SYMBOL(___copy_to_user);
EXPORT_SYMBOL(___copy_from_user);
EXPORT_SYMBOL(___copy_in_user);
EXPORT_SYMBOL(__clear_user);

/* RW semaphores */
EXPORT_SYMBOL(__down_read);
EXPORT_SYMBOL(__down_read_trylock);
EXPORT_SYMBOL(__down_write);
EXPORT_SYMBOL(__down_write_trylock);
EXPORT_SYMBOL(__up_read);
EXPORT_SYMBOL(__up_write);
EXPORT_SYMBOL(__downgrade_write);

/* Atomic counter implementation. */
EXPORT_SYMBOL(atomic_add);
EXPORT_SYMBOL(atomic_add_ret);
EXPORT_SYMBOL(atomic_sub);
EXPORT_SYMBOL(atomic_sub_ret);
EXPORT_SYMBOL(atomic64_add);
EXPORT_SYMBOL(atomic64_add_ret);
EXPORT_SYMBOL(atomic64_sub);
EXPORT_SYMBOL(atomic64_sub_ret);

/* Atomic bit operations. */
EXPORT_SYMBOL(test_and_set_bit);
EXPORT_SYMBOL(test_and_clear_bit);
EXPORT_SYMBOL(test_and_change_bit);
EXPORT_SYMBOL(set_bit);
EXPORT_SYMBOL(clear_bit);
EXPORT_SYMBOL(change_bit);

/* Special internal versions of library functions. */
EXPORT_SYMBOL(_clear_page);
EXPORT_SYMBOL(clear_user_page);
EXPORT_SYMBOL(copy_user_page);

/* RAID code needs this */
void VISenter(void);
EXPORT_SYMBOL(VISenter);

extern void xor_vis_2(unsigned long, unsigned long *, unsigned long *);
extern void xor_vis_3(unsigned long, unsigned long *, unsigned long *,
		unsigned long *);
extern void xor_vis_4(unsigned long, unsigned long *, unsigned long *,
		unsigned long *, unsigned long *);
extern void xor_vis_5(unsigned long, unsigned long *, unsigned long *,
		unsigned long *, unsigned long *, unsigned long *);
EXPORT_SYMBOL(xor_vis_2);
EXPORT_SYMBOL(xor_vis_3);
EXPORT_SYMBOL(xor_vis_4);
EXPORT_SYMBOL(xor_vis_5);

extern void xor_niagara_2(unsigned long, unsigned long *, unsigned long *);
extern void xor_niagara_3(unsigned long, unsigned long *, unsigned long *,
		unsigned long *);
extern void xor_niagara_4(unsigned long, unsigned long *, unsigned long *,
		unsigned long *, unsigned long *);
extern void xor_niagara_5(unsigned long, unsigned long *, unsigned long *,
		unsigned long *, unsigned long *, unsigned long *);

EXPORT_SYMBOL(xor_niagara_2);
EXPORT_SYMBOL(xor_niagara_3);
EXPORT_SYMBOL(xor_niagara_4);
EXPORT_SYMBOL(xor_niagara_5);
#endif
