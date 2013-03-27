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

/* still unfinished */
#define	sys_ioperm		printargs
#define	sys_syslog		printargs
#define	sys_iopl		printargs
#define	sys_vm86old		printargs
#define	sys_get_kernel_syms	printargs
#define	sys_bdflush		printargs
#define	sys_sysfs		printargs
#define	sys_afs_syscall		printargs

/* machine-specific */
#ifndef I386
#define	sys_modify_ldt		printargs
#ifndef M68K
#define sys_get_thread_area	printargs
#define sys_set_thread_area	printargs
#endif
#endif

#define sys_sched_yield		printargs
#define sys_sched_get_priority_max sys_sched_get_priority_min
#define sys_sched_rr_get_interval printargs

/* like another call */
#define	sys_uselib		sys_chdir
#define	sys_umount		sys_chdir
#define	sys_swapon		sys_chdir
#define	sys_swapoff		sys_chdir
#define	sys_delete_module	sys_open
#define	sys_fchdir		sys_close
#define	sys_getgid		sys_getuid
#define	sys_getegid		sys_getuid
#define	sys_geteuid		sys_getuid
#define	sys_setfsgid		sys_setfsuid
#define	sys_acct		sys_chdir
#define sys_fdatasync		sys_close
#define sys_mlock		sys_munmap
#define sys_munlock		sys_munmap
#define sys_clock_getres	sys_clock_gettime
#define sys_mq_unlink		sys_unlink

/* printargs does the right thing */
#define	sys_setup		printargs
#define	sys_getpid		printargs
#define	sys_pause		printargs
#define	sys_sync		printargs
#define	sys_getppid		printargs
#define	sys_getpgrp		printargs
#define	sys_setsid		printargs
#define	sys_vhangup		printargs
#define	sys_idle		printargs
#define	sys_getpgid		printargs
#define sys_munlockall		printargs
#define sys_timer_getoverrun	printargs
#define sys_timer_delete	printargs

/* subcall entry points */
#define	sys_socketcall		printargs
#define	sys_ipc			printargs

/* unimplemented */
#define	sys_stty		printargs
#define	sys_gtty		printargs
#define	sys_ftime		printargs
#define	sys_prof		printargs
#define	sys_phys		printargs
#define	sys_lock		printargs
#define	sys_mpx			printargs
#define	sys_ulimit		printargs
#define	sys_profil		printargs
#define	sys_ustat		printargs
#define	sys_break		printargs

/* deprecated */
#define	sys_olduname		printargs
#define	sys_oldolduname		printargs

/* no library support */
#ifndef HAVE_SENDMSG
#define sys_sendmsg		printargs
#define sys_recvmsg		printargs
#endif

#ifndef SYS_getpmsg
#define sys_getpmsg		printargs
#endif
#ifndef SYS_putpmsg
#define sys_putpmsg		printargs
#endif

#ifndef HAVE_STRUCT___OLD_KERNEL_STAT
#define sys_oldstat		printargs
#define sys_oldfstat		printargs
#define sys_oldlstat		printargs
#endif

#if DONE
#define sys_oldselect		printargs
#define	sys_msync		printargs
#define	sys_flock		printargs
#define	sys_getdents		printargs
#define	sys_stime		printargs
#define	sys_time		printargs
#define	sys_times		printargs
#define	sys_mount		printargs
#define	sys_nice		printargs
#define	sys_mprotect		printargs
#define	sys_sigprocmask		printargs
#define	sys_adjtimex		printargs
#define	sys_sysinfo		printargs
#define	sys_ipc			printargs
#define	sys_setdomainname	printargs
#define	sys_statfs		printargs
#define	sys_fstatfs		printargs
#define	sys_ptrace		printargs
#define	sys_sigreturn		printargs
#define	sys_fsync		printargs
#define	sys_alarm		printargs
#define	sys_socketcall		printargs
#define	sys_sigsuspend		printargs
#define	sys_utime		printargs
#define	sys_brk			printargs
#define	sys_mmap		printargs
#define	sys_munmap		printargs
#define	sys_select		printargs
#define	sys_setuid		printargs
#define	sys_setgid		printargs
#define	sys_setreuid		printargs
#define	sys_setregid		printargs
#define	sys_getgroups		printargs
#define	sys_setgroups		printargs
#define	sys_setrlimit		printargs
#define	sys_getrlimit		printargs
#define	sys_getrusage		printargs
#define	sys_getpriority		printargs
#define	sys_setpriority		printargs
#define	sys_setpgid		printargs
#define	sys_access		printargs
#define	sys_sethostname		printargs
#define	sys_readdir		printargs
#define	sys_waitpid		printargs
#define	sys_wait4		printargs
#define	sys_execve		printargs
#define	sys_fork		printargs
#define	sys_uname		printargs
#define	sys_pipe		printargs
#define	sys_siggetmask		printargs
#define	sys_sigsetmask		printargs
#define	sys_exit		printargs
#define	sys_kill		printargs
#define	sys_signal		printargs
#define	sys_sigaction		printargs
#define	sys_sigpending		printargs
#define	sys_fcntl		printargs
#define	sys_dup			printargs
#define	sys_dup2		printargs
#define	sys_close		printargs
#define	sys_ioctl		printargs
#define	sys_read		printargs
#define	sys_write		printargs
#define	sys_open		printargs
#define	sys_creat		printargs
#define	sys_link		printargs
#define	sys_unlink		printargs
#define	sys_chdir		printargs
#define	sys_mknod		printargs
#define	sys_chmod		printargs
#define	sys_chown		printargs
#define	sys_lseek		printargs
#define	sys_rename		printargs
#define	sys_mkdir		printargs
#define	sys_rmdir		printargs
#define	sys_umask		printargs
#define	sys_chroot		printargs
#define	sys_gettimeofday	printargs
#define	sys_settimeofday	printargs
#define	sys_symlink		printargs
#define	sys_readlink		printargs
#define	sys_truncate		printargs
#define	sys_ftruncate		printargs
#define	sys_fchmod		printargs
#define	sys_fchown		printargs
#define	sys_setitimer		printargs
#define	sys_getitimer		printargs
#define	sys_stat		printargs
#define	sys_lstat		printargs
#define	sys_fstat		printargs
#define	sys_personality		printargs
#define sys_poll		printargs
#define	sys_create_module	printargs
#define	sys_init_module		printargs
#define	sys_quotactl		printargs
#define sys_mlockall		printargs
#define	sys_reboot		printargs
#endif
