/* Set flags signalling availability of kernel features based on given
   kernel version number.
   Copyright (C) 1999-2003, 2004, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* This file must not contain any C code.  At least it must be protected
   to allow using the file also in assembler files.  */

#if defined __mips__
# include <sgidefs.h>
#endif

#include <linux/version.h>
#define __LINUX_KERNEL_VERSION		LINUX_VERSION_CODE

/* We assume for __LINUX_KERNEL_VERSION the same encoding used in
   linux/version.h.  I.e., the major, minor, and subminor all get a
   byte with the major number being in the highest byte.  This means
   we can do numeric comparisons.

   In the following we will define certain symbols depending on
   whether the describes kernel feature is available in the kernel
   version given by __LINUX_KERNEL_VERSION.  We are not always exactly
   recording the correct versions in which the features were
   introduced.  If somebody cares these values can afterwards be
   corrected.  Most of the numbers here are set corresponding to
   2.2.0.  */

/* `getcwd' system call.  */
#if __LINUX_KERNEL_VERSION >= 131584
# define __ASSUME_GETCWD_SYSCALL	1
#endif

/* When was `poll' introduced?  */
#if __LINUX_KERNEL_VERSION >= 131584
# define __ASSUME_POLL_SYSCALL          1
#endif

/* Real-time signal became usable in 2.1.70.  */
#if __LINUX_KERNEL_VERSION >= 131398
# define __ASSUME_REALTIME_SIGNALS	1
#endif

/* When were the `pread'/`pwrite' syscalls introduced?  */
#if __LINUX_KERNEL_VERSION >= 131584
# define __ASSUME_PREAD_SYSCALL		1
# define __ASSUME_PWRITE_SYSCALL	1
#endif

/* When was `poll' introduced?  */
#if __LINUX_KERNEL_VERSION >= 131584
# define __ASSUME_POLL_SYSCALL		1
#endif

/* The `lchown' syscall was introduced in 2.1.80.  */
#if __LINUX_KERNEL_VERSION >= 131408
# define __ASSUME_LCHOWN_SYSCALL	1
#endif

/* When did the `setresuid' sysall became available?  */
#if __LINUX_KERNEL_VERSION >= 131584 && !defined __sparc__
# define __ASSUME_SETRESUID_SYSCALL	1
#endif

/* The SIOCGIFNAME ioctl is available starting with 2.1.50.  */
#if __LINUX_KERNEL_VERSION >= 131408
# define __ASSUME_SIOCGIFNAME		1
#endif

/* MSG_NOSIGNAL was at least available with Linux 2.2.0.  */
#if __LINUX_KERNEL_VERSION >= 131584
# define __ASSUME_MSG_NOSIGNAL		1
#endif

/* On x86 another `getrlimit' syscall was added in 2.3.25.  */
#if __LINUX_KERNEL_VERSION >= 131865 && defined __i386__
# define __ASSUME_NEW_GETRLIMIT_SYSCALL	1
#endif

/* On x86 the truncate64/ftruncate64 syscalls were introduced in 2.3.31.  */
#if __LINUX_KERNEL_VERSION >= 131871 && defined __i386__
# define __ASSUME_TRUNCATE64_SYSCALL	1
#endif

/* On x86 the mmap2 syscall was introduced in 2.3.31.  */
#if __LINUX_KERNEL_VERSION >= 131871 && defined __i386__
# define __ASSUME_MMAP2_SYSCALL	1
#endif

/* On x86 the stat64/lstat64/fstat64 syscalls were introduced in 2.3.34.  */
#if __LINUX_KERNEL_VERSION >= 131874 && defined __i386__
# define __ASSUME_STAT64_SYSCALL	1
#endif

/* On sparc and ARM the truncate64/ftruncate64/mmap2/stat64/lstat64/fstat64
   syscalls were introduced in 2.3.35.  */
#if __LINUX_KERNEL_VERSION >= 131875 && (defined __sparc__ || defined __arm__)
# define __ASSUME_TRUNCATE64_SYSCALL	1
# define __ASSUME_MMAP2_SYSCALL		1
# define __ASSUME_STAT64_SYSCALL	1
#endif

/* I know for sure that getrlimit are in 2.3.35 on powerpc.  */
#if __LINUX_KERNEL_VERSION >= 131875 && defined __powerpc__
# define __ASSUME_NEW_GETRLIMIT_SYSCALL	1
#endif

/* I know for sure that these are in 2.3.35 on powerpc. But PowerPC64 does not
   support separate 64-bit syscalls, already 64-bit */
#if __LINUX_KERNEL_VERSION >= 131875 && defined __powerpc__ \
    && !defined __powerpc64__
# define __ASSUME_TRUNCATE64_SYSCALL	1
# define __ASSUME_STAT64_SYSCALL	1
#endif

/* Linux 2.3.39 introduced 32bit UID/GIDs.  Some platforms had 32
   bit type all along.  */
#if __LINUX_KERNEL_VERSION >= 131879 || defined __powerpc__ || defined __mips__
# define __ASSUME_32BITUIDS		1
#endif

/* Linux 2.3.39 sparc added setresuid.  */
#if __LINUX_KERNEL_VERSION >= 131879 && defined __sparc__
# define __ASSUME_SETRESUID_SYSCALL	1
#endif

#if __LINUX_KERNEL_VERSION >= 131879
# define __ASSUME_SETRESGID_SYSCALL	1
#endif

/* Linux 2.3.39 introduced IPC64.  Except for powerpc.  */
#if __LINUX_KERNEL_VERSION >= 131879 && !defined __powerpc__
# define __ASSUME_IPC64		1
#endif

/* MIPS platforms had IPC64 all along.  */
#if defined __mips__
# define __ASSUME_IPC64		1
#endif

/* We can use the LDTs for threading with Linux 2.3.99 and newer.  */
#if __LINUX_KERNEL_VERSION >= 131939
# define __ASSUME_LDT_WORKS		1
#endif

/* Linux 2.4.0 on PPC introduced a correct IPC64. But PowerPC64 does not
   support a separate 64-bit sys call, already 64-bit */
#if __LINUX_KERNEL_VERSION >= 132096 && defined __powerpc__ \
    && !defined __powerpc64__
# define __ASSUME_IPC64			1
#endif

/* SH kernels got stat64, mmap2, and truncate64 during 2.4.0-test.  */
#if __LINUX_KERNEL_VERSION >= 132096 && defined __sh__
# define __ASSUME_TRUNCATE64_SYSCALL	1
# define __ASSUME_MMAP2_SYSCALL		1
# define __ASSUME_STAT64_SYSCALL	1
#endif

/* The changed st_ino field appeared in 2.4.0-test6.  But we cannot
   distinguish this version from other 2.4.0 releases.  Therefore play
   save and assume it available is for 2.4.1 and up.  However, SH is lame,
   and still does not have a 64-bit inode field.  */
#if __LINUX_KERNEL_VERSION >= 132097 && !defined __alpha__ && !defined __sh__
# define __ASSUME_ST_INO_64_BIT		1
#endif

/* To support locking of large files a new fcntl() syscall was introduced
   in 2.4.0-test7.  We test for 2.4.1 for the earliest version we know
   the syscall is available.  */
#if __LINUX_KERNEL_VERSION >= 132097 && (defined __i386__ || defined __sparc__)
# define __ASSUME_FCNTL64		1
#endif

/* The AT_CLKTCK auxiliary vector entry was introduction in the 2.4.0
   series.  */
#if __LINUX_KERNEL_VERSION >= 132097
# define __ASSUME_AT_CLKTCK		1
#endif

/* Arm got fcntl64 in 2.4.4, PowerPC and SH have it also in 2.4.4 (I
   don't know when it got introduced).  But PowerPC64 does not support
   separate FCNTL64 call, FCNTL is already 64-bit */
#if __LINUX_KERNEL_VERSION >= 132100 \
    && (defined __arm__ || defined __powerpc__ || defined __sh__) \
    && !defined __powerpc64__
# define __ASSUME_FCNTL64		1
#endif

/* The getdents64 syscall was introduced in 2.4.0-test7.  We test for
   2.4.1 for the earliest version we know the syscall is available.  */
#if __LINUX_KERNEL_VERSION >= 132097
# define __ASSUME_GETDENTS64_SYSCALL	1
#endif

/* When did O_DIRECTORY became available?  Early in 2.3 but when?
   Be safe, use 2.3.99.  */
#if __LINUX_KERNEL_VERSION >= 131939
# define __ASSUME_O_DIRECTORY		1
#endif

/* Starting with one of the 2.4.0 pre-releases the Linux kernel passes
   up the page size information.  */
#if __LINUX_KERNEL_VERSION >= 132097
# define __ASSUME_AT_PAGESIZE		1
#endif

/* Starting with at least 2.4.0 the kernel passes the uid/gid unconditionally
   up to the child.  */
#if __LINUX_KERNEL_VERSION >= 132097
# define __ASSUME_AT_XID		1
#endif

/* Starting with 2.4.5 kernels PPC passes the AUXV in the standard way
   and the vfork syscall made it into the official kernel.  */
#if __LINUX_KERNEL_VERSION >= (132096+5) && defined __powerpc__
# define __ASSUME_STD_AUXV		1
# define __ASSUME_VFORK_SYSCALL		1
#endif

/* Starting with 2.4.5 kernels the mmap2 syscall made it into the official
   kernel.  But PowerPC64 does not support a separate MMAP2 call.  */
#if __LINUX_KERNEL_VERSION >= (132096+5) && defined __powerpc__ \
    && !defined __powerpc64__
# define __ASSUME_MMAP2_SYSCALL		1
#endif

/* Starting with 2.4.21 PowerPC implements the new prctl syscall.
   This allows applications to get/set the Floating Point Exception Mode.  */
#if __LINUX_KERNEL_VERSION >= (132096+21) && defined __powerpc__
# define __ASSUME_NEW_PRCTL_SYSCALL		1
#endif

/* Starting with 2.4.21 the PowerPC32 clone syscall works as expected.  */
#if __LINUX_KERNEL_VERSION >= (132096+21) && defined __powerpc__ \
    && !defined __powerpc64__
# define __ASSUME_FIXED_CLONE_SYSCALL		1
#endif

/* Starting with 2.4.21 PowerPC64 implements the new rt_sigreturn syscall.
   The new rt_sigreturn takes an ucontext pointer allowing rt_sigreturn
   to be used in the set/swapcontext implementation.  */
#if __LINUX_KERNEL_VERSION >= (132096+21) && defined __powerpc64__
# define __ASSUME_NEW_RT_SIGRETURN_SYSCALL		1
#endif

/* On x86, the set_thread_area syscall was introduced in 2.5.29, but its
   semantics was changed in 2.5.30, and again after 2.5.31.  */
#if __LINUX_KERNEL_VERSION >= 132384 && defined __i386__
# define __ASSUME_SET_THREAD_AREA_SYSCALL	1
#endif

/* The vfork syscall on x86 and arm was definitely available in 2.4.  */
#if __LINUX_KERNEL_VERSION >= 132097 && (defined __i386__ || defined __arm__)
# define __ASSUME_VFORK_SYSCALL		1
#endif

/* There are an infinite number of PA-RISC kernel versions numbered
   2.4.0.  But they've not really been released as such.  We require
   and expect the final version here.  */
#ifdef __hppa__
# define __ASSUME_32BITUIDS		1
# define __ASSUME_TRUNCATE64_SYSCALL	1
# define __ASSUME_MMAP2_SYSCALL		1
# define __ASSUME_STAT64_SYSCALL	1
# define __ASSUME_IPC64			1
# define __ASSUME_ST_INO_64_BIT		1
# define __ASSUME_FCNTL64		1
# define __ASSUME_GETDENTS64_SYSCALL	1
#endif

/* Alpha switched to a 64-bit timeval sometime before 2.2.0.  */
#if __LINUX_KERNEL_VERSION >= 131584 && defined __alpha__
# define __ASSUME_TIMEVAL64		1
#endif

#if defined __mips__ && _MIPS_SIM == _ABIN32
# define __ASSUME_FCNTL64		1
#endif

/* The late 2.5 kernels saw a lot of new CLONE_* flags.  Summarize
   their availability with one define.  The changes were made first
   for i386 and the have to be done separately for the other archs.
   For i386 we pick 2.5.50 as the first version with support.  */
#if __LINUX_KERNEL_VERSION >= 132402 && defined __i386__
# define __ASSUME_CLONE_THREAD_FLAGS	1
#endif

/* Support for various CLOEXEC and NONBLOCK flags was added for x86,
   x86-64, PPC, IA-64, SPARC< and S390 in 2.6.23.  */
#if __LINUX_KERNEL_VERSION >= 0x020617 \
    && (defined __i386__ || defined __x86_64__ || defined __powerpc__ \
	|| defined __ia64__ || defined __sparc__ || defined __s390__)
# define __ASSUME_O_CLOEXEC 1
#endif

/* Support for various CLOEXEC and NONBLOCK flags was added for x86,
 *    x86-64, PPC, IA-64, and SPARC in 2.6.27.  */
#if __LINUX_KERNEL_VERSION >= 0x02061b \
    && (defined __i386__ || defined __x86_64__ || defined __powerpc__ \
        || defined __ia64__ || defined __sparc__ || defined __s390__)
/* # define __ASSUME_SOCK_CLOEXEC  1 */
/* # define __ASSUME_IN_NONBLOCK   1 */
# define __ASSUME_PIPE2         1
/* # define __ASSUME_EVENTFD2      1 */
/* # define __ASSUME_SIGNALFD4     1 */
#endif


/* These features were surely available with 2.4.12.  */
#if __LINUX_KERNEL_VERSION >= 132108 && defined __mc68000__
# define __ASSUME_MMAP2_SYSCALL		1
# define __ASSUME_TRUNCATE64_SYSCALL	1
# define __ASSUME_STAT64_SYSCALL	1
# define __ASSUME_FCNTL64		1
# define __ASSUME_VFORK_SYSCALL		1
#endif

/* Beginning with 2.5.63 support for realtime and monotonic clocks and
   timers based on them is available.  */
#if __LINUX_KERNEL_VERSION >= 132415
# define __ASSUME_POSIX_TIMERS		1
#endif

/* Beginning with 2.6.12 the clock and timer supports CPU clocks.  */
#if __LINUX_KERNEL_VERSION >= 0x2060c
# define __ASSUME_POSIX_CPU_TIMERS      1
#endif

/* The late 2.5 kernels saw a lot of new CLONE_* flags.  Summarize
   their availability with one define.  The changes were made first
   for i386 and the have to be done separately for the other archs.
   For ia64, s390*, PPC, x86-64 we pick 2.5.64 as the first version
   with support.  */
#if __LINUX_KERNEL_VERSION >= 132416 \
    && (defined __ia64__ || defined __s390__ || defined __powerpc__ \
	|| defined __x86_64__ || defined __sh__)
# define __ASSUME_CLONE_THREAD_FLAGS	1
#endif

/* With kernel 2.4.17 we always have netlink support.  */
#if __LINUX_KERNEL_VERSION >= (132096+17)
# define __ASSUME_NETLINK_SUPPORT	1
#endif

/* The requeue futex functionality was introduced in 2.5.70.  */
#if __LINUX_KERNEL_VERSION >= 132422
# define __ASSUME_FUTEX_REQUEUE	1
#endif

/* The statfs64 syscalls are available in 2.5.74.  */
#if __LINUX_KERNEL_VERSION >= 132426
# define __ASSUME_STATFS64	1
#endif

/* Starting with at least 2.5.74 the kernel passes the setuid-like exec
   flag unconditionally up to the child.  */
#if __LINUX_KERNEL_VERSION >= 132426
# define __ASSUME_AT_SECURE	1
#endif

/* Starting with the 2.5.75 kernel the kernel fills in the correct value
   in the si_pid field passed as part of the siginfo_t struct to signal
   handlers.  */
#if __LINUX_KERNEL_VERSION >= 132427
# define __ASSUME_CORRECT_SI_PID	1
#endif

/* The tgkill syscall was instroduced for i386 in 2.5.75.  For Alpha
   it was introduced in 2.6.0-test1 which unfortunately cannot be
   distinguished from 2.6.0.  On x86-64, ppc, and ppc64 it was
   introduced in 2.6.0-test3. */
#if (__LINUX_KERNEL_VERSION >= 132427 && defined __i386__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __alpha__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __x86_64__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __powerpc__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __sh__)
# define __ASSUME_TGKILL	1
#endif

/* The utimes syscall has been available for some architectures
   forever.  For x86 it was introduced after 2.5.75, for x86-64,
   ppc, and ppc64 it was introduced in 2.6.0-test3.  */
#if defined __alpha__ || defined __ia64__ || defined __hppa__ \
    || defined __sparc__ \
    || (__LINUX_KERNEL_VERSION > 132427 && defined __i386__) \
    || (__LINUX_KERNEL_VERSION > 132609 && defined __x86_64__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __powerpc__) \
    || (__LINUX_KERNEL_VERSION >= 132609 && defined __sh__)
# define __ASSUME_UTIMES	1
#endif

// XXX Disabled for now since the semantics we want is not achieved.
#if 0
/* The CLONE_STOPPED flag was introduced in the 2.6.0-test1 series.  */
#if __LINUX_KERNEL_VERSION >= 132609
# define __ASSUME_CLONE_STOPPED	1
#endif
#endif

/* The fixed version of the posix_fadvise64 syscall appeared in
   2.6.0-test3.  At least for x86.  Powerpc support appeared in
   2.6.2, but for 32-bit userspace only.  */
#if (__LINUX_KERNEL_VERSION >= 132609 && defined __i386__) \
    || (__LINUX_KERNEL_VERSION >= 132610 && defined __powerpc__ \
       && !defined __powerpc64__)
# define __ASSUME_FADVISE64_64_SYSCALL	1
#endif

/* The PROT_GROWSDOWN/PROT_GROWSUP flags were introduced in the 2.6.0-test
   series.  */
#if __LINUX_KERNEL_VERSION >= 132609
# define __ASSUME_PROT_GROWSUPDOWN	1
#endif

/* Starting with 2.6.0 PowerPC adds signal/swapcontext support for Vector
   SIMD (AKA Altivec, VMX) instructions and register state.  This changes
   the overall size of the sigcontext and adds the swapcontext syscall.  */
#if __LINUX_KERNEL_VERSION >= 132608 && defined __powerpc__
# define __ASSUME_SWAPCONTEXT_SYSCALL	1
#endif

/* The CLONE_DETACHED flag is not necessary in 2.6.2 kernels, it is
   implied.  */
#if __LINUX_KERNEL_VERSION >= 132610
# define __ASSUME_NO_CLONE_DETACHED	1
#endif

/* Starting with version 2.6.4-rc1 the getdents syscall returns d_type
   information as well and in between 2.6.5 and 2.6.8 most compat wrappers
   were fixed too.  Except s390{,x} which was fixed in 2.6.11.  */
#if (__LINUX_KERNEL_VERSION >= 0x020608 && !defined __s390__) \
    || (__LINUX_KERNEL_VERSION >= 0x02060b && defined __s390__)
# define __ASSUME_GETDENTS32_D_TYPE	1
#endif

/* Starting with version 2.5.3, the initial location returned by `brk'
   after exec is always rounded up to the next page.  */
#if __LINUX_KERNEL_VERSION >= 132355
# define __ASSUME_BRK_PAGE_ROUNDED	1
#endif

/* Starting with version 2.6.9, the waitid system call is available.
   Except for powerpc and powerpc64, where it is available in 2.6.12.  */
#if (__LINUX_KERNEL_VERSION >= 0x020609 && !defined __powerpc__) \
    || (__LINUX_KERNEL_VERSION >= 0x02060c && defined __powerpc__)
# define __ASSUME_WAITID_SYSCALL	1
#endif

/* Starting with version 2.6.9, SSI_IEEE_RAISE_EXCEPTION exists.  */
#if __LINUX_KERNEL_VERSION >= 0x020609 && defined __alpha__
#define __ASSUME_IEEE_RAISE_EXCEPTION	1
#endif

/* Support for the accept4 syscall was added in 2.6.28.  */
#if __LINUX_KERNEL_VERSION >= 0x02061c \
    && (defined __i386__ || defined __x86_64__ || defined __powerpc__ \
        || defined __sparc__ || defined __s390__)
# define __ASSUME_ACCEPT4       1
#endif

/* Support for the accept4 syscall for alpha was added after 2.6.33-rc1.  */
#if __LINUX_KERNEL_VERSION >= 0x020621 && defined __alpha__
# define __ASSUME_ACCEPT4       1
#endif

/* Support for the FUTEX_CLOCK_REALTIME flag was added in 2.6.29.  */
#if __LINUX_KERNEL_VERSION >= 0x02061d
# define __ASSUME_FUTEX_CLOCK_REALTIME	1
#endif

/* Support for PI futexes was added in 2.6.18.  */
#if __LINUX_KERNEL_VERSION >= 0x020612
# define __ASSUME_FUTEX_LOCK_PI	1
#endif

/* Support for private futexes was added in 2.6.22.  */
#if __LINUX_KERNEL_VERSION >= 0x020616
# define __ASSUME_PRIVATE_FUTEX	1
#endif


