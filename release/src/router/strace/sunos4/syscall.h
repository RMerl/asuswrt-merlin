/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
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

int	sys_nosys();
int	sys_nullsys();
int	sys_errsys();

/* 1.1 processes and protection */
int	sys_gethostid(),sys_sethostname(),sys_gethostname(),sys_getpid();
int	sys_setdomainname(),sys_getdomainname();
int	sys_fork(),sys_exit(),sys_execv(),sys_execve(),sys_wait4();
int	sys_getuid(),sys_setreuid(),sys_getgid(),sys_getgroups(),sys_setregid(),sys_setgroups();
int	sys_getpgrp(),sys_setpgrp();
int	sys_sys_setsid(), sys_setpgid();
int	sys_uname();

/* 1.2 memory management */
int	sys_brk(),sys_sbrk(),sys_sstk();
int	sys_getpagesize(),sys_mmap(),sys_mctl(),sys_munmap(),sys_mprotect(),sys_mincore();
int	sys_omsync(),sys_omadvise();

/* 1.3 signals */
int	sys_sigvec(),sys_sigblock(),sys_sigsetmask(),sys_sigpause(),sys_sigstack(),sys_sigcleanup();
int	sys_kill(), sys_killpg(), sys_sigpending();

/* 1.4 timing and statistics */
int	sys_gettimeofday(),sys_settimeofday();
int	sys_adjtime();
int	sys_getitimer(),sys_setitimer();

/* 1.5 descriptors */
int	sys_getdtablesize(),sys_dup(),sys_dup2(),sys_close();
int	sys_select(),sys_getdopt(),sys_setdopt(),sys_fcntl(),sys_flock();

/* 1.6 resource controls */
int	sys_getpriority(),sys_setpriority(),sys_getrusage(),sys_getrlimit(),sys_setrlimit();
int	sys_oldquota(), sys_quotactl();
int	sys_rtschedule();

/* 1.7 system operation support */
int	sys_mount(),sys_unmount(),sys_swapon();
int	sys_sync(),sys_reboot();
int	sys_sysacct();
int	sys_auditsys();

/* 2.1 generic operations */
int	sys_read(),sys_write(),sys_readv(),sys_writev(),sys_ioctl();

/* 2.1.1 asynch operations */
int	sys_aioread(), sys_aiowrite(), sys_aiowait(), sys_aiocancel();

/* 2.2 file system */
int	sys_chdir(),sys_chroot();
int	sys_fchdir(),sys_fchroot();
int	sys_mkdir(),sys_rmdir(),sys_getdirentries(), sys_getdents();
int	sys_creat(),sys_open(),sys_mknod(),sys_unlink(),sys_stat(),sys_fstat(),sys_lstat();
int	sys_chown(),sys_fchown(),sys_chmod(),sys_fchmod(),sys_utimes();
int	sys_link(),sys_symlink(),sys_readlink(),sys_rename();
int	sys_lseek(),sys_truncate(),sys_ftruncate(),sys_access(),sys_fsync();
int	sys_statfs(),sys_fstatfs();

/* 2.3 communications */
int	sys_socket(),sys_bind(),sys_listen(),sys_accept(),sys_connect();
int	sys_socketpair(),sys_sendto(),sys_send(),sys_recvfrom(),sys_recv();
int	sys_sendmsg(),sys_recvmsg(),sys_shutdown(),sys_setsockopt(),sys_getsockopt();
int	sys_getsockname(),sys_getpeername(),sys_pipe();

int	sys_umask();		/* XXX */

/* 2.3.1 SystemV-compatible IPC */
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
#define	SYS_shmat		(SYS_shmsys_subcall + 0)
#define SYS_shmctl		(SYS_shmsys_subcall + 1)
#define SYS_shmdt		(SYS_shmsys_subcall + 2)
#define SYS_shmget		(SYS_shmsys_subcall + 3)

/* 2.4 processes */
int	sys_ptrace();

/* 2.5 terminals */

/* emulations for backwards compatibility */
int	sys_otime();		/* now use gettimeofday */
int	sys_ostime();		/* now use settimeofday */
int	sys_oalarm();		/* now use setitimer */
int	sys_outime();		/* now use utimes */
int	sys_opause();		/* now use sigpause */
int	sys_onice();		/* now use setpriority,getpriority */
int	sys_oftime();		/* now use gettimeofday */
int	sys_osetpgrp();		/* ??? */
int	sys_otimes();		/* now use getrusage */
int	sys_ossig();		/* now use sigvec, etc */
int	sys_ovlimit();		/* now use setrlimit,getrlimit */
int	sys_ovtimes();		/* now use getrusage */
int	sys_osetuid();		/* now use setreuid */
int	sys_osetgid();		/* now use setregid */
int	sys_ostat();		/* now use stat */
int	sys_ofstat();		/* now use fstat */

/* BEGIN JUNK */
int	sys_profil();		/* 'cuz sys calls are interruptible */
int	sys_vhangup();		/* should just do in sys_exit() */
int	sys_vfork();		/* XXX - was awaiting fork w/ copy on write */
int	sys_ovadvise();		/* awaiting new madvise */
int	sys_indir();		/* indirect system call */
int	sys_ustat();		/* System V compatibility */
int	sys_owait();		/* should use wait4 interface */
int	sys_owait3();		/* should use wait4 interface */
int	sys_umount();		/* still more Sys V (and 4.2?) compatibility */
int	sys_pathconf();		/* posix */
int	sys_fpathconf();		/* posix */
int	sys_sysconf();		/* posix */

int sys_debug();
/* END JUNK */

int	sys_vtrace();		/* kernel event tracing */

/* nfs */
int	sys_async_daemon();		/* client async daemon */
int	sys_nfs_svc();		/* run nfs server */
int	sys_nfs_getfh();		/* get file handle */
int	sys_exportfs();		/* export file systems */

int  	sys_rfssys();		/* RFS-related calls */

int	sys_getmsg();
int	sys_putmsg();
int	sys_poll();

int	sys_vpixsys();		/* VP/ix system calls */
