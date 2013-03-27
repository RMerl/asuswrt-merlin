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

#define	SOLARIS_syscall	0
#define	SOLARIS_exit	1
#define	SOLARIS_fork	2
#define	SOLARIS_read	3
#define	SOLARIS_write	4
#define	SOLARIS_open	5
#define	SOLARIS_close	6
#define	SOLARIS_wait	7
#define	SOLARIS_creat	8
#define	SOLARIS_link	9
#define	SOLARIS_unlink	10
#define	SOLARIS_exec	11
#define	SOLARIS_chdir	12
#define	SOLARIS_time	13
#define	SOLARIS_mknod	14
#define	SOLARIS_chmod	15
#define	SOLARIS_chown	16
#define	SOLARIS_brk		17
#define	SOLARIS_stat	18
#define	SOLARIS_lseek	19
#define	SOLARIS_getpid	20
#define	SOLARIS_mount	21
#define	SOLARIS_umount	22
#define	SOLARIS_setuid	23
#define	SOLARIS_getuid	24
#define	SOLARIS_stime	25
#define	SOLARIS_ptrace	26
#define	SOLARIS_alarm	27
#define	SOLARIS_fstat	28
#define	SOLARIS_pause	29
#define	SOLARIS_utime	30
#define	SOLARIS_stty	31
#define	SOLARIS_gtty	32
#define	SOLARIS_access	33
#define	SOLARIS_nice	34
#define	SOLARIS_statfs	35
#define	SOLARIS_sync	36
#define	SOLARIS_kill	37
#define	SOLARIS_fstatfs	38
#define	SOLARIS_pgrpsys	39
#define	SOLARIS_xenix	40
#define	SOLARIS_dup		41
#define	SOLARIS_pipe	42
#define	SOLARIS_times	43
#define	SOLARIS_profil	44
#define	SOLARIS_plock	45
#define	SOLARIS_setgid	46
#define	SOLARIS_getgid	47
#define	SOLARIS_signal	48
#define	SOLARIS_msgsys	49
#define	SOLARIS_syssun	50
#define	SOLARIS_acct	51
#define	SOLARIS_shmsys	52
#define	SOLARIS_semsys	53
#define	SOLARIS_ioctl	54
#define	SOLARIS_uadmin	55
#define	SOLARIS_utssys	57
#define	SOLARIS_fdsync	58
#define	SOLARIS_execve	59
#define	SOLARIS_umask	60
#define	SOLARIS_chroot	61
#define	SOLARIS_fcntl	62
#define	SOLARIS_ulimit	63
#define	SOLARIS_rmdir	79
#define	SOLARIS_mkdir	80
#define	SOLARIS_getdents	81
#define	SOLARIS_sysfs	84
#define	SOLARIS_getmsg	85
#define	SOLARIS_putmsg	86
#define	SOLARIS_poll	87
#define	SOLARIS_lstat	88
#define	SOLARIS_symlink	89
#define	SOLARIS_readlink	90
#define	SOLARIS_setgroups	91
#define	SOLARIS_getgroups	92
#define	SOLARIS_fchmod	93
#define	SOLARIS_fchown	94
#define	SOLARIS_sigprocmask	95
#define	SOLARIS_sigsuspend	96
#define	SOLARIS_sigaltstack	97
#define	SOLARIS_sigaction	98
#define	SOLARIS_sigpending	99
#define	SOLARIS_context	100
#define	SOLARIS_evsys	101
#define	SOLARIS_evtrapret	102
#define	SOLARIS_statvfs	103
#define	SOLARIS_fstatvfs	104
#define	SOLARIS_nfssys	106
#define	SOLARIS_waitsys	107
#define	SOLARIS_sigsendsys	108
#define	SOLARIS_hrtsys	109
#define	SOLARIS_acancel	110
#define	SOLARIS_async	111
#define	SOLARIS_priocntlsys	112
#define	SOLARIS_pathconf	113
#define	SOLARIS_mincore	114
#define	SOLARIS_mmap	115
#define	SOLARIS_mprotect	116
#define	SOLARIS_munmap	117
#define	SOLARIS_fpathconf	118
#define	SOLARIS_vfork	119
#define	SOLARIS_fchdir	120
#define	SOLARIS_readv	121
#define	SOLARIS_writev	122
#define	SOLARIS_xstat	123
#define	SOLARIS_lxstat	124
#define	SOLARIS_fxstat	125
#define	SOLARIS_xmknod	126
#define	SOLARIS_clocal	127
#define	SOLARIS_setrlimit	128
#define	SOLARIS_getrlimit	129
#define	SOLARIS_lchown	130
#define	SOLARIS_memcntl	131
#define	SOLARIS_getpmsg	132
#define	SOLARIS_putpmsg	133
#define	SOLARIS_rename	134
#define	SOLARIS_uname	135
#define	SOLARIS_setegid	136
#define	SOLARIS_sysconfig	137
#define	SOLARIS_adjtime	138
#define	SOLARIS_systeminfo	139
#define	SOLARIS_seteuid	141
#define	SOLARIS_vtrace	142
#define	SOLARIS_fork1	143
#define	SOLARIS_sigtimedwait	144
#define	SOLARIS_lwp_info	145
#define	SOLARIS_yield	146
#define	SOLARIS_lwp_sema_wait	147
#define	SOLARIS_lwp_sema_post	148
#define	SOLARIS_modctl	152
#define	SOLARIS_fchroot	153
#define	SOLARIS_utimes	154
#define	SOLARIS_vhangup	155
#define	SOLARIS_gettimeofday	156
#define	SOLARIS_getitimer		157
#define	SOLARIS_setitimer		158
#define	SOLARIS_lwp_create		159
#define	SOLARIS_lwp_exit		160
#define	SOLARIS_lwp_suspend		161
#define	SOLARIS_lwp_continue	162
#define	SOLARIS_lwp_kill		163
#define	SOLARIS_lwp_self		164
#define	SOLARIS_lwp_setprivate	165
#define	SOLARIS_lwp_getprivate	166
#define	SOLARIS_lwp_wait		167
#define	SOLARIS_lwp_mutex_unlock	168
#define	SOLARIS_lwp_mutex_lock	169
#define	SOLARIS_lwp_cond_wait	170
#define	SOLARIS_lwp_cond_signal	171
#define	SOLARIS_lwp_cond_broadcast	172
#define	SOLARIS_pread		173
#define	SOLARIS_pwrite		174
#define	SOLARIS_llseek		175
#define	SOLARIS_inst_sync		176
#define	SOLARIS_kaio		178
#define	SOLARIS_tsolsys		184
#define	SOLARIS_acl			185
#define	SOLARIS_auditsys		186
#define	SOLARIS_processor_bind	187
#define	SOLARIS_processor_info	188
#define	SOLARIS_p_online		189
#define	SOLARIS_sigqueue		190
#define	SOLARIS_clock_gettime	191
#define	SOLARIS_clock_settime	192
#define	SOLARIS_clock_getres	193
#define	SOLARIS_timer_create	194
#define	SOLARIS_timer_delete	195
#define	SOLARIS_timer_settime	196
#define	SOLARIS_timer_gettime	197
#define	SOLARIS_timer_getoverrun	198
#define	SOLARIS_nanosleep		199
#define	SOLARIS_facl		200
#define	SOLARIS_door		201
#define	SOLARIS_setreuid		202
#define	SOLARIS_setregid		203
#define	SOLARIS_signotifywait	210
#define	SOLARIS_lwp_sigredirect	211
#define	SOLARIS_lwp_alarm		212

#include "dummy2.h"

extern int solaris_syscall();
extern int solaris_exit();
extern int solaris_fork();
extern int solaris_read();
extern int solaris_write();
extern int solaris_open();
extern int solaris_close();
extern int solaris_wait();
extern int solaris_creat();
extern int solaris_link();
extern int solaris_unlink();
extern int solaris_exec();
extern int solaris_chdir();
extern int solaris_time();
extern int solaris_mknod();
extern int solaris_chmod();
extern int solaris_chown();
extern int solaris_brk();
extern int solaris_stat();
extern int solaris_lseek();
extern int solaris_getpid();
extern int solaris_mount();
extern int solaris_umount();
extern int solaris_setuid();
extern int solaris_getuid();
extern int solaris_stime();
extern int solaris_ptrace();
extern int solaris_alarm();
extern int solaris_fstat();
extern int solaris_pause();
extern int solaris_utime();
extern int solaris_stty();
extern int solaris_gtty();
extern int solaris_access();
extern int solaris_nice();
extern int solaris_statfs();
extern int solaris_sync();
extern int solaris_kill();
extern int solaris_fstatfs();
extern int solaris_pgrpsys();
extern int solaris_setpgrp();
extern int solaris_xenix();
extern int solaris_syssgi();
extern int solaris_dup();
extern int solaris_pipe();
extern int solaris_times();
extern int solaris_profil();
extern int solaris_plock();
extern int solaris_setgid();
extern int solaris_getgid();
extern int solaris_sigcall();
extern int solaris_msgsys();
extern int solaris_syssun();
extern int solaris_sysi86();
extern int solaris_sysmips();
extern int solaris_sysmachine();
extern int solaris_acct();
extern int solaris_shmsys();
extern int solaris_semsys();
extern int solaris_ioctl();
extern int solaris_uadmin();
extern int solaris_utssys();
extern int solaris_fdsync();
extern int solaris_execve();
extern int solaris_umask();
extern int solaris_chroot();
extern int solaris_fcntl();
extern int solaris_ulimit();
extern int solaris_rmdir();
extern int solaris_mkdir();
extern int solaris_getdents();
extern int solaris_sysfs();
extern int solaris_getmsg();
extern int solaris_putmsg();
extern int solaris_poll();
extern int solaris_lstat();
extern int solaris_symlink();
extern int solaris_readlink();
extern int solaris_setgroups();
extern int solaris_getgroups();
extern int solaris_fchmod();
extern int solaris_fchown();
extern int solaris_sigprocmask();
extern int solaris_sigsuspend();
extern int solaris_sigaltstack();
extern int solaris_sigaction();
extern int solaris_spcall();
extern int solaris_context();
extern int solaris_evsys();
extern int solaris_evtrapret();
extern int solaris_statvfs();
extern int solaris_fstatvfs();
extern int solaris_nfssys();
extern int solaris_waitid();
extern int solaris_sigsendsys();
extern int solaris_hrtsys();
extern int solaris_acancel();
extern int solaris_async();
extern int solaris_priocntlsys();
extern int solaris_pathconf();
extern int solaris_mincore();
extern int solaris_mmap();
extern int solaris_mprotect();
extern int solaris_munmap();
extern int solaris_fpathconf();
extern int solaris_vfork();
extern int solaris_fchdir();
extern int solaris_readv();
extern int solaris_writev();
extern int solaris_xstat();
extern int solaris_lxstat();
extern int solaris_fxstat();
extern int solaris_xmknod();
extern int solaris_clocal();
extern int solaris_setrlimit();
extern int solaris_getrlimit();
extern int solaris_lchown();
extern int solaris_memcntl();
extern int solaris_getpmsg();
extern int solaris_putpmsg();
extern int solaris_rename();
extern int solaris_uname();
extern int solaris_setegid();
extern int solaris_sysconfig();
extern int solaris_adjtime();
extern int solaris_sysinfo();
extern int solaris_seteuid();
extern int solaris_vtrace();
extern int solaris_fork1();
extern int solaris_sigtimedwait();
extern int solaris_lwp_info();
extern int solaris_yield();
extern int solaris_lwp_sema_wait();
extern int solaris_lwp_sema_post();
extern int solaris_modctl();
extern int solaris_fchroot();
extern int solaris_utimes();
extern int solaris_vhangup();
extern int solaris_gettimeofday();
extern int solaris_getitimer();
extern int solaris_setitimer();
extern int solaris_lwp_create();
extern int solaris_lwp_exit();
extern int solaris_lwp_suspend();
extern int solaris_lwp_continue();
extern int solaris_lwp_kill();
extern int solaris_lwp_self();
extern int solaris_lwp_setprivate();
extern int solaris_lwp_getprivate();
extern int solaris_lwp_wait();
extern int solaris_lwp_mutex_unlock();
extern int solaris_lwp_mutex_lock();
extern int solaris_lwp_cond_wait();
extern int solaris_lwp_cond_signal();
extern int solaris_lwp_cond_broadcast();
extern int solaris_pread();
extern int solaris_pwrite();
extern int solaris_llseek();
extern int solaris_inst_sync();
extern int solaris_auditsys();
extern int solaris_processor_bind();
extern int solaris_processor_info();
extern int solaris_p_online();
extern int solaris_sigqueue();
extern int solaris_clock_gettime();
extern int solaris_clock_settime();
extern int solaris_clock_getres();
extern int solaris_timer_create();
extern int solaris_timer_delete();
extern int solaris_timer_settime();
extern int solaris_timer_gettime();
extern int solaris_timer_getoverrun();
extern int solaris_nanosleep();

/* solaris_pgrpsys subcalls */

extern int solaris_getpgrp(), solaris_setpgrp(), solaris_getsid();
extern int solaris_setsid(), solaris_getpgid(), solaris_setpgid();

#define SOLARIS_pgrpsys_subcall	300
#define SOLARIS_getpgrp		(SOLARIS_pgrpsys_subcall + 0)
#define SOLARIS_setpgrp		(SOLARIS_pgrpsys_subcall + 1)
#define SOLARIS_getsid		(SOLARIS_pgrpsys_subcall + 2)
#define SOLARIS_setsid		(SOLARIS_pgrpsys_subcall + 3)
#define SOLARIS_getpgid		(SOLARIS_pgrpsys_subcall + 4)
#define SOLARIS_setpgid		(SOLARIS_pgrpsys_subcall + 5)

#define SOLARIS_pgrpsys_nsubcalls	6

/* solaris_sigcall subcalls */

#undef SOLARIS_signal
#define SOLARIS_sigcall		48

extern int solaris_signal(), solaris_sigset(), solaris_sighold();
extern int solaris_sigrelse(), solaris_sigignore(), solaris_sigpause();

#define SOLARIS_sigcall_subcall	310
#define SOLARIS_signal		(SOLARIS_sigcall_subcall + 0)
#define SOLARIS_sigset		(SOLARIS_sigcall_subcall + 1)
#define SOLARIS_sighold		(SOLARIS_sigcall_subcall + 2)
#define SOLARIS_sigrelse		(SOLARIS_sigcall_subcall + 3)
#define SOLARIS_sigignore		(SOLARIS_sigcall_subcall + 4)
#define SOLARIS_sigpause		(SOLARIS_sigcall_subcall + 5)

#define SOLARIS_sigcall_nsubcalls	6

/* msgsys subcalls */

extern int solaris_msgget(), solaris_msgctl(), solaris_msgrcv(), solaris_msgsnd();

#define SOLARIS_msgsys_subcall	320
#define SOLARIS_msgget		(SOLARIS_msgsys_subcall + 0)
#define SOLARIS_msgctl		(SOLARIS_msgsys_subcall + 1)
#define SOLARIS_msgrcv		(SOLARIS_msgsys_subcall + 2)
#define SOLARIS_msgsnd		(SOLARIS_msgsys_subcall + 3)

#define SOLARIS_msgsys_nsubcalls	4

/* shmsys subcalls */

extern int solaris_shmat(), solaris_shmctl(), solaris_shmdt(), solaris_shmget();

#define SOLARIS_shmsys_subcall	330
#define SOLARIS_shmat		(SOLARIS_shmsys_subcall + 0)
#define SOLARIS_shmctl		(SOLARIS_shmsys_subcall + 1)
#define SOLARIS_shmdt		(SOLARIS_shmsys_subcall + 2)
#define SOLARIS_shmget		(SOLARIS_shmsys_subcall + 3)

#define SOLARIS_shmsys_nsubcalls	4

/* semsys subcalls */

extern int solaris_semctl(), solaris_semget(), solaris_semop();

#define SOLARIS_semsys_subcall	340
#define SOLARIS_semctl		(SOLARIS_semsys_subcall + 0)
#define SOLARIS_semget		(SOLARIS_semsys_subcall + 1)
#define SOLARIS_semop		(SOLARIS_semsys_subcall + 2)

#define SOLARIS_semsys_nsubcalls	3

/* utssys subcalls */

extern int solaris_olduname(), solaris_ustat(), solaris_fusers();

#define SOLARIS_utssys_subcall	350

#define SOLARIS_olduname		(SOLARIS_utssys_subcall + 0)
				/* 1 is unused */
#define SOLARIS_ustat		(SOLARIS_utssys_subcall + 2)
#define SOLARIS_fusers		(SOLARIS_utssys_subcall + 3)

#define SOLARIS_utssys_nsubcalls	4

/* sysfs subcalls */

extern int solaris_sysfs1(), solaris_sysfs2(), solaris_sysfs3();

#define SOLARIS_sysfs_subcall	360
				/* 0 is unused */
#define SOLARIS_sysfs1		(SOLARIS_sysfs_subcall + 1)
#define SOLARIS_sysfs2		(SOLARIS_sysfs_subcall + 2)
#define SOLARIS_sysfs3		(SOLARIS_sysfs_subcall + 3)

#define SOLARIS_sysfs_nsubcalls	4

/* solaris_spcall subcalls */

#undef SOLARIS_sigpending
#define SOLARIS_spcall		99

extern int solaris_sigpending(), solaris_sigfillset();

#define SOLARIS_spcall_subcall	370
				/* 0 is unused */
#define SOLARIS_sigpending		(SOLARIS_spcall_subcall + 1)
#define SOLARIS_sigfillset		(SOLARIS_spcall_subcall + 2)

#define SOLARIS_spcall_nsubcalls	3

/* solaris_context subcalls */

extern int solaris_getcontext(), solaris_setcontext();

#define SOLARIS_context_subcall	380
#define SOLARIS_getcontext		(SOLARIS_context_subcall + 0)
#define SOLARIS_setcontext		(SOLARIS_context_subcall + 1)

#define SOLARIS_context_nsubcalls	2
