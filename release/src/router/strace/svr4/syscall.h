/*
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

extern int sys_syscall();
extern int sys_exit();
extern int sys_fork();
extern int sys_read();
extern int sys_write();
extern int sys_open();
extern int sys_close();
extern int sys_wait();
extern int sys_creat();
extern int sys_link();
extern int sys_unlink();
extern int sys_exec();
extern int sys_chdir();
extern int sys_time();
extern int sys_settimeofday();
extern int sys_mknod();
extern int sys_chmod();
extern int sys_chown();
extern int sys_brk();
extern int sys_stat();
extern int sys_lseek();
extern int sys_getpid();
extern int sys_mount();
extern int sys_umount();
extern int sys_setuid();
extern int sys_getuid();
extern int sys_stime();
extern int sys_ptrace();
extern int sys_alarm();
extern int sys_fstat();
extern int sys_pause();
extern int sys_utime();
extern int sys_stty();
extern int sys_gtty();
extern int sys_access();
extern int sys_nice();
extern int sys_statfs();
extern int sys_sync();
extern int sys_kill();
extern int sys_fstatfs();
extern int sys_pgrpsys();
extern int sys_setpgrp();
extern int sys_xenix();
extern int sys_syssgi();
extern int sys_dup();
extern int sys_pipe();
extern int sys_times();
extern int sys_profil();
extern int sys_plock();
extern int sys_setgid();
extern int sys_getgid();
extern int sys_sigcall();
extern int sys_msgsys();
extern int sys_syssun();
extern int sys_sysi86();
extern int sys_sysmips();
extern int sys_sysmachine();
extern int sys_acct();
extern int sys_shmsys();
extern int sys_semsys();
extern int sys_ioctl();
extern int sys_uadmin();
extern int sys_utssys();
extern int sys_fdsync();
extern int sys_execve();
extern int sys_umask();
extern int sys_chroot();
extern int sys_fcntl();
extern int sys_ulimit();
extern int sys_rmdir();
extern int sys_mkdir();
extern int sys_getdents();
extern int sys_sysfs();
extern int sys_getmsg();
extern int sys_putmsg();
extern int sys_poll();
extern int sys_dup2();
extern int sys_bind();
extern int sys_listen();
extern int sys_accept();
extern int sys_connect();
extern int sys_shutdown();
extern int sys_recv();
extern int sys_recvfrom();
extern int sys_send();
extern int sys_sendto();
extern int sys_getpeername();
extern int sys_getsockname();
extern int sys_getsockopt();
extern int sys_setsockopt();
#ifdef MIPS
extern int sys_sigreturn();
extern int sys_gethostid();
extern int sys_recvmsg();
extern int sys_select();
extern int sys_sendmsg();
extern int sys_sethostid();
extern int sys_socket();
extern int sys_listen();
extern int sys_gethostname();
extern int sys_sethostname();
extern int sys_getdomainname();
extern int sys_setdomainname();
extern int sys_truncate();
extern int sys_ftruncate();
extern int sys_rename();
extern int sys_symlink();
extern int sys_readlink();
extern int sys_nfssvc();
extern int sys_getfh();
extern int sys_async_daemon();
extern int sys_exportfs();
extern int sys_setregid();
extern int sys_setreuid();
extern int sys_getitimer();
extern int sys_setitimer();
extern int sys_adjtime();
extern int sys_BSD_getime();
extern int sys_sproc();
extern int sys_prctl();
extern int sys_procblk();
extern int sys_sprocsp();
extern int sys_mmap();
extern int sys_munmap();
extern int sys_mprotect();
extern int sys_msync();
extern int sys_madvise();
extern int sys_pagelock();
extern int sys_getpagesize();
extern int sys_quotactl();
extern int sys_BSDgetpgrp();
extern int sys_BSDsetpgrp();
extern int sys_vhangup();
extern int sys_fsync();
extern int sys_fchdir();
extern int sys_getrlimit();
extern int sys_setrlimit();
extern int sys_cacheflush();
extern int sys_cachectl();
extern int sys_fchown();
extern int sys_fchmod();
extern int sys_socketpair();
extern int sys_sysinfo();
extern int sys_nuname();
extern int sys_xstat();
extern int sys_lxstat();
extern int sys_fxstat();
extern int sys_xmknod();
extern int sys_ksigaction();
extern int sys_sigpending();
extern int sys_sigprocmask();
extern int sys_sigsuspend();
extern int sys_sigpoll();
extern int sys_swapctl();
extern int sys_getcontext();
extern int sys_setcontext();
extern int sys_waitsys();
extern int sys_sigstack();
extern int sys_sigaltstack();
extern int sys_sigsendset();
extern int sys_statvfs();
extern int sys_fstatvfs();
extern int sys_getpmsg();
extern int sys_putpmsg();
extern int sys_lchown();
extern int sys_priocntl();
extern int sys_ksigqueue();
#else /* !MIPS */
extern int sys_lstat();
extern int sys_symlink();
extern int sys_readlink();
extern int sys_setgroups();
extern int sys_getgroups();
extern int sys_fchmod();
extern int sys_fchown();
extern int sys_sigprocmask();
extern int sys_sigsuspend();
extern int sys_sigaltstack();
extern int sys_sigaction();
extern int sys_spcall();
extern int sys_context();
extern int sys_evsys();
extern int sys_evtrapret();
extern int sys_statvfs();
extern int sys_fstatvfs();
extern int sys_nfssys();
extern int sys_waitid();
extern int sys_sigsendsys();
extern int sys_hrtsys();
extern int sys_acancel();
extern int sys_async();
extern int sys_priocntlsys();
extern int sys_pathconf();
extern int sys_mincore();
extern int sys_mmap();
extern int sys_mprotect();
extern int sys_munmap();
extern int sys_fpathconf();
extern int sys_vfork();
extern int sys_fchdir();
extern int sys_readv();
extern int sys_writev();
extern int sys_xstat();
extern int sys_lxstat();
extern int sys_fxstat();
extern int sys_xmknod();
extern int sys_clocal();
extern int sys_setrlimit();
extern int sys_getrlimit();
extern int sys_lchown();
extern int sys_memcntl();
extern int sys_getpmsg();
extern int sys_putpmsg();
extern int sys_rename();
extern int sys_uname();
extern int sys_setegid();
extern int sys_sysconfig();
extern int sys_adjtime();
extern int sys_sysinfo();
extern int sys_seteuid();
extern int sys_vtrace();
extern int sys_fork1();
extern int sys_sigtimedwait();
extern int sys_lwp_info();
extern int sys_yield();
extern int sys_lwp_sema_wait();
extern int sys_lwp_sema_post();
extern int sys_modctl();
extern int sys_fchroot();
extern int sys_utimes();
extern int sys_vhangup();
extern int sys_gettimeofday();
extern int sys_getitimer();
extern int sys_setitimer();
extern int sys_lwp_create();
extern int sys_lwp_exit();
extern int sys_lwp_suspend();
extern int sys_lwp_continue();
extern int sys_lwp_kill();
extern int sys_lwp_self();
extern int sys_lwp_setprivate();
extern int sys_lwp_getprivate();
extern int sys_lwp_wait();
extern int sys_lwp_mutex_unlock();
extern int sys_lwp_mutex_lock();
extern int sys_lwp_cond_wait();
extern int sys_lwp_cond_signal();
extern int sys_lwp_cond_broadcast();
extern int sys_pread();
extern int sys_pwrite();
extern int sys_inst_sync();
extern int sys_auditsys();
extern int sys_processor_bind();
extern int sys_processor_info();
extern int sys_p_online();
extern int sys_sigqueue();
extern int sys_clock_gettime();
extern int sys_clock_settime();
extern int sys_clock_getres();
extern int sys_timer_create();
extern int sys_timer_delete();
extern int sys_timer_settime();
extern int sys_timer_gettime();
extern int sys_timer_getoverrun();
extern int sys_nanosleep();
extern int sys_setreuid();
extern int sys_setregid();
#ifdef HAVE_SYS_ACL_H
extern int sys_acl();
extern int sys_facl();
extern int sys_aclipc();
#endif
#ifdef HAVE_SYS_DOOR_H
extern int sys_door();
#endif
#if UNIXWARE >= 2
extern int sys_sigwait();
extern int sys_truncate();
extern int sys_ftruncate();
extern int sys_getksym ();
extern int sys_procpriv();
#endif
#if UNIXWARE >= 7
extern int sys_lseek64 ();
extern int sys_truncate64 ();
extern int sys_ftruncate64 ();
extern int sys_xsocket ();
extern int sys_xsocketpair ();
extern int sys_xbind ();
extern int sys_xconnect ();
extern int sys_xlisten ();
extern int sys_xaccept ();
extern int sys_xrecvmsg ();
extern int sys_xsendmsg ();
extern int sys_xgetsockaddr ();
extern int sys_xsetsockaddr ();
extern int sys_xgetsockopt ();
extern int sys_xsetsockopt ();
extern int sys_xshutdown ();
extern int sys_rfork ();
extern int sys_ssisys ();
extern int sys_rexecve ();
#endif
#endif /* !MIPS */

#ifdef MIPS
#define SGI_KLUDGE 1
#else
#define SGI_KLUDGE 0
#endif

/* sys_pgrpsys subcalls */

extern int sys_getpgrp(), sys_setpgrp(), sys_getsid();
extern int sys_setsid(), sys_getpgid(), sys_setpgid();

#ifndef MIPS

#define SYS_pgrpsys_subcall	300 + SGI_KLUDGE
#define SYS_getpgrp		(SYS_pgrpsys_subcall + 0)
#define SYS_setpgrp		(SYS_pgrpsys_subcall + 1)
#define SYS_getsid		(SYS_pgrpsys_subcall + 2)
#define SYS_setsid		(SYS_pgrpsys_subcall + 3)
#define SYS_getpgid		(SYS_pgrpsys_subcall + 4)
#define SYS_setpgid		(SYS_pgrpsys_subcall + 5)

#define SYS_pgrpsys_nsubcalls	6

#endif /* !MIPS */

/* sys_sigcall subcalls */

#undef SYS_signal
#define SYS_sigcall		48

extern int sys_signal(), sys_sigset(), sys_sighold();
extern int sys_sigrelse(), sys_sigignore(), sys_sigpause();

#ifndef MIPS

#define SYS_sigcall_subcall	310 + SGI_KLUDGE
#define SYS_signal		(SYS_sigcall_subcall + 0)
#define SYS_sigset		(SYS_sigcall_subcall + 1)
#define SYS_sighold		(SYS_sigcall_subcall + 2)
#define SYS_sigrelse		(SYS_sigcall_subcall + 3)
#define SYS_sigignore		(SYS_sigcall_subcall + 4)
#define SYS_sigpause		(SYS_sigcall_subcall + 5)

#define SYS_sigcall_nsubcalls	6

#endif /* !MIPS */

/* msgsys subcalls */

extern int sys_msgget(), sys_msgctl(), sys_msgrcv(), sys_msgsnd();

#define SYS_msgsys_subcall	320 + SGI_KLUDGE
#define SYS_msgget		(SYS_msgsys_subcall + 0)
#define SYS_msgctl		(SYS_msgsys_subcall + 1)
#define SYS_msgrcv		(SYS_msgsys_subcall + 2)
#define SYS_msgsnd		(SYS_msgsys_subcall + 3)

#define SYS_msgsys_nsubcalls	4

/* shmsys subcalls */

extern int sys_shmat(), sys_shmctl(), sys_shmdt(), sys_shmget();

#define SYS_shmsys_subcall	330 + SGI_KLUDGE
#define SYS_shmat		(SYS_shmsys_subcall + 0)
#define SYS_shmctl		(SYS_shmsys_subcall + 1)
#define SYS_shmdt		(SYS_shmsys_subcall + 2)
#define SYS_shmget		(SYS_shmsys_subcall + 3)

#define SYS_shmsys_nsubcalls	4

/* semsys subcalls */

extern int sys_semctl(), sys_semget(), sys_semop();

#define SYS_semsys_subcall	340 + SGI_KLUDGE
#define SYS_semctl		(SYS_semsys_subcall + 0)
#define SYS_semget		(SYS_semsys_subcall + 1)
#define SYS_semop		(SYS_semsys_subcall + 2)

#define SYS_semsys_nsubcalls	3

/* utssys subcalls */

extern int sys_olduname(), sys_ustat(), sys_fusers();

#define SYS_utssys_subcall	350 + SGI_KLUDGE

#define SYS_olduname		(SYS_utssys_subcall + 0)
				/* 1 is unused */
#define SYS_ustat		(SYS_utssys_subcall + 2)
#define SYS_fusers		(SYS_utssys_subcall + 3)

#define SYS_utssys_nsubcalls	4

/* sysfs subcalls */

extern int sys_sysfs1(), sys_sysfs2(), sys_sysfs3();

#define SYS_sysfs_subcall	360 + SGI_KLUDGE
				/* 0 is unused */
#define SYS_sysfs1		(SYS_sysfs_subcall + 1)
#define SYS_sysfs2		(SYS_sysfs_subcall + 2)
#define SYS_sysfs3		(SYS_sysfs_subcall + 3)

#define SYS_sysfs_nsubcalls	4

/* sys_spcall subcalls */

#undef SYS_sigpending
#define SYS_spcall		99

extern int sys_sigpending(), sys_sigfillset();

#define SYS_spcall_subcall	370 + SGI_KLUDGE
				/* 0 is unused */
#define SYS_sigpending		(SYS_spcall_subcall + 1)
#define SYS_sigfillset		(SYS_spcall_subcall + 2)

#define SYS_spcall_nsubcalls	3

/* sys_context subcalls */

extern int sys_getcontext(), sys_setcontext();

#ifndef MIPS

#define SYS_context_subcall	380 + SGI_KLUDGE
#define SYS_getcontext		(SYS_context_subcall + 0)
#define SYS_setcontext		(SYS_context_subcall + 1)

#define SYS_context_nsubcalls	2

#endif /* !MIPS */


#ifdef HAVE_SYS_AIO_H
extern int sys_aioread();
extern int sys_aiowrite();
extern int sys_aiowait();
extern int sys_aiocancel();
#endif /* HAVE_SYS_AIO_H */

/* 64-bit file stuff */

#if _LFS64_LARGEFILE
extern int sys_getdents64();
extern int sys_mmap64();
extern int sys_stat64();
extern int sys_lstat64();
extern int sys_fstat64();
extern int sys_setrlimit64();
extern int sys_getrlimit64();
extern int sys_pread64();
extern int sys_pwrite64();
extern int sys_lseek64();
#endif

/* solaris 2.6 stuff */
extern int sys_so_socket();
extern int sys_so_socketpair();

#ifdef HAVE_SYS_DOOR_H

#define SYS_door_subcall	390 + SGI_KLUDGE
#define SYS_door_create		(SYS_door_subcall + 0)
#define SYS_door_revoke		(SYS_door_subcall + 1)
#define SYS_door_info		(SYS_door_subcall + 2)
#define SYS_door_call		(SYS_door_subcall + 3)
#define SYS_door_return		(SYS_door_subcall + 4)
#define SYS_door_cred		(SYS_door_subcall + 5)

#define SYS_door_nsubcalls	6

#endif /* HAVE_SYS_DOOR_H */

#ifdef HAVE_SYS_AIO_H

#define SYS_kaio_subcall	400 + SGI_KLUDGE
#define SYS_aioread		(SYS_kaio_subcall + 0)
#define SYS_aiowrite		(SYS_kaio_subcall + 1)
#define SYS_aiowait		(SYS_kaio_subcall + 2)
#define SYS_aiocancel		(SYS_kaio_subcall + 3)
#define SYS_aionotify		(SYS_kaio_subcall + 4)
#define SYS_aioinit		(SYS_kaio_subcall + 5)
#define SYS_aiostart		(SYS_kaio_subcall + 6)
#define SYS_aiolio		(SYS_kaio_subcall + 7)
#define SYS_aiosuspend		(SYS_kaio_subcall + 8)
#define SYS_aioerror		(SYS_kaio_subcall + 9)
#define SYS_aioliowait		(SYS_kaio_subcall + 10)
#define SYS_aioaread		(SYS_kaio_subcall + 11)
#define SYS_aioawrite		(SYS_kaio_subcall + 12)
#define SYS_aiolio64		(SYS_kaio_subcall + 13)
#define SYS_aiosuspend64	(SYS_kaio_subcall + 14)
#define SYS_aioerror64		(SYS_kaio_subcall + 15)
#define SYS_aioliowait64	(SYS_kaio_subcall + 16)
#define SYS_aioaread64		(SYS_kaio_subcall + 17)
#define SYS_aioawrite64		(SYS_kaio_subcall + 18)
#define SYS_aiocancel64		(SYS_kaio_subcall + 19)
#define SYS_aiofsync		(SYS_kaio_subcall + 20)

#define SYS_kaio_nsubcalls	21

#endif /* HAVE_SYS_AIO_H */
