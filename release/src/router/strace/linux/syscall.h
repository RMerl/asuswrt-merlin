/*
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995 Rick Sladkey <jrs@world.std.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

#include "dummy.h"

/* primary syscalls */

int sys_restart_syscall();
int sys_setup(), sys_exit(), sys_fork(), sys_read(), sys_write();
int sys_open(), sys_close(), sys_waitpid(), sys_creat(), sys_link();
int sys_unlink(), sys_execve(), sys_chdir(), sys_time(), sys_mknod();
int sys_chmod(), sys_chown(), sys_break(), sys_oldstat();
int sys_lseek(), sys_getpid(), sys_mount(), sys_umount(), sys_umount2();
int sys_setuid(), sys_getuid(), sys_stime(), sys_ptrace();
int sys_alarm(), sys_oldfstat(), sys_pause(), sys_utime();
int sys_stty(), sys_gtty(), sys_access(), sys_nice(), sys_ftime();
int sys_sync(), sys_kill(), sys_rename(), sys_mkdir(), sys_rmdir();
int sys_dup(), sys_pipe(), sys_times(), sys_prof(), sys_brk();
int sys_setgid(), sys_getgid(), sys_signal(), sys_geteuid();
int sys_getegid(), sys_acct(), sys_phys(), sys_lock(), sys_ioctl();
int sys_fcntl(), sys_mpx(), sys_setpgid(), sys_ulimit();
int sys_olduname(), sys_umask(), sys_chroot(), sys_ustat();
int sys_dup2(), sys_getppid(), sys_getpgrp(), sys_setsid();
int sys_sigaction(), sys_siggetmask(), sys_sigsetmask();
int sys_setreuid(), sys_setregid(), sys_sigsuspend();
int sys_sigpending(), sys_sethostname(), sys_setrlimit();
int sys_getrlimit(), sys_getrusage(), sys_gettimeofday();
int sys_settimeofday(), sys_getgroups(), sys_setgroups();
int sys_setgroups32(), sys_getgroups32();
int sys_oldselect(), sys_symlink(), sys_oldlstat(), sys_readlink();
int sys_uselib(), sys_swapon(), sys_reboot(), sys_readdir();
int sys_mmap(), sys_munmap(), sys_truncate(), sys_ftruncate();
int sys_fchmod(), sys_fchown(), sys_getpriority();
int sys_setpriority(), sys_profil(), sys_statfs(), sys_fstatfs();
int sys_ioperm(), sys_socketcall(), sys_syslog(), sys_setitimer();
int sys_getitimer(), sys_stat(), sys_lstat(), sys_fstat();
int sys_uname(), sys_iopl(), sys_vhangup(), sys_idle(), sys_vm86();
int sys_wait4(), sys_swapoff(), sys_ipc(), sys_sigreturn();
int sys_fsync(), sys_clone(), sys_setdomainname(), sys_sysinfo();
int sys_modify_ldt(), sys_adjtimex(), sys_mprotect();
int sys_sigprocmask(), sys_create_module(), sys_init_module();
int sys_delete_module(), sys_get_kernel_syms(), sys_quotactl();
int sys_getpgid(), sys_fchdir(), sys_bdflush();
int sys_sysfs(), sys_personality(), sys_afs_syscall();
int sys_setfsuid(), sys_setfsgid(), sys_llseek();
int sys_getdents(), sys_flock(), sys_msync();
int sys_readv(), sys_writev(), sys_select();
int sys_getsid(), sys_fdatasync(), sys_sysctl();
int sys_mlock(), sys_munlock(), sys_mlockall(), sys_munlockall(), sys_madvise();
int sys_sched_setparam(), sys_sched_getparam();
int sys_sched_setscheduler(), sys_sched_getscheduler(), sys_sched_yield();
int sys_sched_get_priority_max(), sys_sched_get_priority_min();
int sys_sched_rr_get_interval(), sys_nanosleep(), sys_mremap();
int sys_sendmsg(), sys_recvmsg(), sys_setresuid(), sys_setresgid();
int sys_getresuid(), sys_getresgid(), sys_pread(), sys_pwrite(), sys_getcwd();
int sys_sigaltstack(), sys_rt_sigprocmask(), sys_rt_sigaction();
int sys_rt_sigpending(), sys_rt_sigsuspend(), sys_rt_sigqueueinfo();
int sys_rt_sigtimedwait(), sys_prctl(), sys_poll(), sys_vfork();
int sys_sendfile(), sys_old_mmap(), sys_stat64(), sys_lstat64(), sys_fstat64();
int sys_truncate64(), sys_ftruncate64(), sys_pivotroot();
int sys_getdents64();
int sys_getpmsg(), sys_putpmsg(), sys_readahead(), sys_sendfile64();
int sys_setxattr(), sys_fsetxattr(), sys_getxattr(), sys_fgetxattr();
int sys_listxattr(), sys_flistxattr(), sys_removexattr(), sys_fremovexattr();
int sys_sched_setaffinity(), sys_sched_getaffinity(), sys_futex();
int sys_set_thread_area(), sys_get_thread_area(), sys_remap_file_pages();
int sys_timer_create(), sys_timer_delete(), sys_timer_getoverrun();
int sys_timer_gettime(), sys_timer_settime(), sys_clock_settime();
int sys_clock_gettime(), sys_clock_getres(), sys_clock_nanosleep();
int sys_semtimedop(), sys_statfs64(), sys_fstatfs64(), sys_tgkill();
int sys_mq_open(), sys_mq_timedsend(), sys_mq_timedreceive();
int sys_mq_notify(), sys_mq_getsetattr();
int sys_epoll_create(), sys_epoll_ctl(), sys_epoll_wait();
int sys_waitid(), sys_fadvise64(), sys_fadvise64_64();
int sys_mbind(), sys_get_mempolicy(), sys_set_mempolicy(), sys_move_pages();
int sys_arch_prctl();
int sys_io_setup(), sys_io_submit(), sys_io_cancel(), sys_io_getevents(), sys_io_destroy();
int sys_utimensat(), sys_epoll_pwait(), sys_signalfd(), sys_timerfd(), sys_eventfd();
int sys_getcpu();
int sys_fallocate(), sys_timerfd_create(), sys_timerfd_settime(), sys_timerfd_gettime();
int sys_signalfd4(), sys_eventfd2(), sys_epoll_create1(), sys_dup3(), sys_pipe2();

/* sys_socketcall subcalls */

int sys_socket(), sys_bind(), sys_connect(), sys_listen(), sys_accept4();
int sys_accept(), sys_getsockname(), sys_getpeername(), sys_socketpair();
int sys_send(), sys_recv(), sys_sendto(), sys_recvfrom();
int sys_shutdown(), sys_setsockopt(), sys_getsockopt();
int sys_recvmmsg();

/* *at syscalls */
int sys_fchmodat();
int sys_newfstatat();
int sys_unlinkat();
int sys_fchownat();
int sys_openat();
int sys_renameat();
int sys_symlinkat();
int sys_readlinkat();
int sys_linkat();
int sys_faccessat();
int sys_mkdirat();
int sys_mknodat();
int sys_futimesat();

/* new ones */
int sys_query_module();
int sys_poll();
int sys_mincore();
int sys_inotify_add_watch();
int sys_inotify_rm_watch();
int sys_inotify_init1();
int sys_pselect6();
int sys_ppoll();
int sys_unshare();

/* architecture-specific calls */
#ifdef ALPHA
int sys_osf_select();
int sys_osf_gettimeofday();
int sys_osf_settimeofday();
int sys_osf_getitimer();
int sys_osf_setitimer();
int sys_osf_getrusage();
int sys_osf_wait4();
int sys_osf_utimes();
#endif


#ifndef SYS_waitid
# ifdef I386
#  define SYS_waitid 284
# elif defined ALPHA
#  define SYS_waitid 438
# elif defined ARM
#  define SYS_waitid (NR_SYSCALL_BASE + 280)
# elif defined IA64
#  define SYS_waitid 1270
# elif defined M68K
#  define SYS_waitid 277
# elif defined POWERPC
#  define SYS_waitid 272
# elif defined S390 || defined S390X
#  define SYS_waitid 281
# elif defined SH64
#  define SYS_waitid 312
# elif defined SH64
#  define SYS_waitid 312
# elif defined SH
#  define SYS_waitid 284
# elif defined SPARC || defined SPARC64
#  define SYS_waitid 279
# elif defined X86_64
#  define SYS_waitid 247
# endif
#endif

#if !defined(ALPHA) && !defined(MIPS) && !defined(HPPA) && \
	!defined(__ARM_EABI__)
# ifdef	IA64
/*
 *  IA64 syscall numbers (the only ones available from standard header
 *  files) are disjoint from IA32 syscall numbers.  We need to define
 *  the IA32 socket call number here.
 */
#  define SYS_socketcall	102

#  undef SYS_socket
#  undef SYS_bind
#  undef SYS_connect
#  undef SYS_listen
#  undef SYS_accept
#  undef SYS_getsockname
#  undef SYS_getpeername
#  undef SYS_socketpair
#  undef SYS_send
#  undef SYS_recv
#  undef SYS_sendto
#  undef SYS_recvfrom
#  undef SYS_shutdown
#  undef SYS_setsockopt
#  undef SYS_getsockopt
#  undef SYS_sendmsg
#  undef SYS_recvmsg
# endif /* IA64 */
# if defined(SPARC) || defined(SPARC64)
#  define SYS_socket_subcall	353
# else
#  define SYS_socket_subcall	400
# endif
#define SYS_sub_socket		(SYS_socket_subcall + 1)
#define SYS_sub_bind		(SYS_socket_subcall + 2)
#define SYS_sub_connect		(SYS_socket_subcall + 3)
#define SYS_sub_listen		(SYS_socket_subcall + 4)
#define SYS_sub_accept		(SYS_socket_subcall + 5)
#define SYS_sub_getsockname	(SYS_socket_subcall + 6)
#define SYS_sub_getpeername	(SYS_socket_subcall + 7)
#define SYS_sub_socketpair	(SYS_socket_subcall + 8)
#define SYS_sub_send		(SYS_socket_subcall + 9)
#define SYS_sub_recv		(SYS_socket_subcall + 10)
#define SYS_sub_sendto		(SYS_socket_subcall + 11)
#define SYS_sub_recvfrom	(SYS_socket_subcall + 12)
#define SYS_sub_shutdown	(SYS_socket_subcall + 13)
#define SYS_sub_setsockopt	(SYS_socket_subcall + 14)
#define SYS_sub_getsockopt	(SYS_socket_subcall + 15)
#define SYS_sub_sendmsg		(SYS_socket_subcall + 16)
#define SYS_sub_recvmsg		(SYS_socket_subcall + 17)
#define SYS_sub_accept4		(SYS_socket_subcall + 18)
#define SYS_sub_recvmmsg	(SYS_socket_subcall + 19)

#define SYS_socket_nsubcalls	20
#endif /* !(ALPHA || MIPS || HPPA) */

/* sys_ipc subcalls */

int sys_semget(), sys_semctl(), sys_semop();
int sys_msgsnd(), sys_msgrcv(), sys_msgget(), sys_msgctl();
int sys_shmat(), sys_shmdt(), sys_shmget(), sys_shmctl();

#if !defined(ALPHA) && !defined(MIPS) && !defined(HPPA) && \
	!defined(__ARM_EABI__)
# ifdef	IA64
   /*
    * IA64 syscall numbers (the only ones available from standard
    * header files) are disjoint from IA32 syscall numbers.  We need
    * to define the IA32 socket call number here.  Fortunately, this
    * symbol, `SYS_ipc', is not used by any of the IA64 code so
    * re-defining this symbol will not cause a problem.
   */
#  undef SYS_ipc
#  define SYS_ipc		117
#  undef SYS_semop
#  undef SYS_semget
#  undef SYS_semctl
#  undef SYS_semtimedop
#  undef SYS_msgsnd
#  undef SYS_msgrcv
#  undef SYS_msgget
#  undef SYS_msgctl
#  undef SYS_shmat
#  undef SYS_shmdt
#  undef SYS_shmget
#  undef SYS_shmctl
# endif /* IA64 */
#define SYS_ipc_subcall		((SYS_socket_subcall)+(SYS_socket_nsubcalls))
#define SYS_sub_semop		(SYS_ipc_subcall + 1)
#define SYS_sub_semget		(SYS_ipc_subcall + 2)
#define SYS_sub_semctl		(SYS_ipc_subcall + 3)
#define SYS_sub_semtimedop	(SYS_ipc_subcall + 4)
#define SYS_sub_msgsnd		(SYS_ipc_subcall + 11)
#define SYS_sub_msgrcv		(SYS_ipc_subcall + 12)
#define SYS_sub_msgget		(SYS_ipc_subcall + 13)
#define SYS_sub_msgctl		(SYS_ipc_subcall + 14)
#define SYS_sub_shmat		(SYS_ipc_subcall + 21)
#define SYS_sub_shmdt		(SYS_ipc_subcall + 22)
#define SYS_sub_shmget		(SYS_ipc_subcall + 23)
#define SYS_sub_shmctl		(SYS_ipc_subcall + 24)

#define SYS_ipc_nsubcalls	25
#endif /* !(ALPHA || MIPS || HPPA) */

#if defined SYS_ipc_subcall && !defined SYS_ipc
# define SYS_ipc SYS_ipc_subcall
#endif
#if defined SYS_socket_subcall && !defined SYS_socketcall
# define SYS_socketcall SYS_socket_subcall
#endif

#ifdef IA64
  /*
   * IA64 syscall numbers (the only ones available from standard header
   * files) are disjoint from IA32 syscall numbers.  We need to define
   * some IA32 specific syscalls here.
   */
# define SYS_fork	2
# define SYS_vfork	190
# define SYS32_exit	1
# define SYS_waitpid	7
# define SYS32_wait4	114
# define SYS32_execve	11
#endif /* IA64 */

#if defined(ALPHA) || defined(IA64)
int sys_getpagesize();
#endif

#ifdef ALPHA
int osf_statfs(), osf_fstatfs();
#endif

#ifdef IA64
int sys_getpmsg(), sys_putpmsg();	/* STREAMS stuff */
#endif

#ifdef MIPS
int sys_sysmips();
#endif

int sys_setpgrp(), sys_gethostname(), sys_getdtablesize(), sys_utimes();
int sys_capget(), sys_capset();

#if defined M68K || defined SH
int sys_cacheflush();
#endif

int sys_pread64(), sys_pwrite64();

#ifdef POWERPC
int sys_subpage_prot();
#endif

#ifdef BFIN
int sys_sram_alloc();
int sys_cacheflush();
#endif

#if defined SPARC || defined SPARC64
#include "sparc/syscall1.h"
int sys_execv();
int sys_getpagesize();
int sys_getmsg(), sys_putmsg();

int	sys_semsys(), sys_semctl(), sys_semget();
#define SYS_semsys_subcall	200
#define SYS_semsys_nsubcalls	3
#define SYS_semctl		(SYS_semsys_subcall + 0)
#define SYS_semget		(SYS_semsys_subcall + 1)
#define SYS_semop		(SYS_semsys_subcall + 2)
int	sys_msgsys(), sys_msgget(), sys_msgctl(), sys_msgrcv(), sys_msgsnd();
#define SYS_msgsys_subcall	203
#define SYS_msgsys_nsubcalls	4
#define SYS_msgget		(SYS_msgsys_subcall + 0)
#define SYS_msgctl		(SYS_msgsys_subcall + 1)
#define SYS_msgrcv		(SYS_msgsys_subcall + 2)
#define SYS_msgsnd		(SYS_msgsys_subcall + 3)
int	sys_shmsys(), sys_shmat(), sys_shmctl(), sys_shmdt(), sys_shmget();
#define SYS_shmsys_subcall	207
#define SYS_shmsys_nsubcalls	4
#define SYS_shmat		(SYS_shmsys_subcall + 0)
#define SYS_shmctl		(SYS_shmsys_subcall + 1)
#define SYS_shmdt		(SYS_shmsys_subcall + 2)
#define SYS_shmget		(SYS_shmsys_subcall + 3)
#endif
