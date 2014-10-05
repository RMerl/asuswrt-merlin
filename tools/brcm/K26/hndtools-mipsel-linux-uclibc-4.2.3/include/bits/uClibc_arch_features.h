/*
 * Track misc arch-specific features that aren't config options
 */

#ifndef _BITS_UCLIBC_ARCH_FEATURES_H
#define _BITS_UCLIBC_ARCH_FEATURES_H

/* instruction used when calling abort() to kill yourself */
#define __UCLIBC_ABORT_INSTRUCTION__ "break 255"

/* can your target use syscall6() for mmap ? */
#define __UCLIBC_MMAP_HAS_6_ARGS__

/* does your target use syscall4() for truncate64 ? (32bit arches only) */
#define __UCLIBC_TRUNCATE64_HAS_4_ARGS__

/* does your target have a broken create_module() ? */
#undef __UCLIBC_BROKEN_CREATE_MODULE__

/* does your target prefix all symbols with an _ ? */
#define __UCLIBC_NO_UNDERSCORES__

/* does your target have an asm .set ? */
#undef __UCLIBC_HAVE_ASM_SET_DIRECTIVE__

/* define if target doesn't like .global */
#undef __UCLIBC_ASM_GLOBAL_DIRECTIVE__

/* define if target supports .weak */
#define __UCLIBC_HAVE_ASM_WEAK_DIRECTIVE__

/* define if target supports .weakext */
#undef __UCLIBC_HAVE_ASM_WEAKEXT_DIRECTIVE__

/* needed probably only for ppc64 */
#undef __UCLIBC_HAVE_ASM_GLOBAL_DOT_NAME__

/* define if target supports IEEE signed zero floats */
#define __UCLIBC_HAVE_SIGNED_ZERO__

#endif /* _BITS_UCLIBC_ARCH_FEATURES_H */
