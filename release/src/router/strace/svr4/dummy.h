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

/* still unfinished */

#define sys_sysmp printargs
#define sys_sginap printargs
#define sys_sgikopt printargs
#define sys_sysmips printargs
#define sys_sigreturn printargs
#define sys_recvmsg printargs
#define sys_sendmsg printargs
#define sys_nfssvc printargs
#define sys_getfh printargs
#define sys_async_daemon printargs
#define sys_exportfs printargs
#define sys_BSD_getime printargs
#define sys_sproc printargs
#define sys_procblk printargs
#define sys_sprocsp printargs
#define sys_msync printargs
#define sys_madvise printargs
#define sys_pagelock printargs
#define sys_quotactl printargs
#define sys_cacheflush printargs
#define sys_cachectl printargs
#define sys_nuname printargs
#define sys_sigpoll printargs
#define sys_swapctl printargs
#define sys_sigstack printargs
#define sys_sigsendset printargs
#define sys_priocntl printargs
#define sys_ksigqueue printargs
#define sys_lwp_sema_wait printargs
#define sys_lwp_sema_trywait printargs
#define sys_syscall printargs
#define sys_clocal printargs
#define sys_syssun printargs
#define sys_sysi86 printargs
#define sys_sysmachine printargs
#define sys_plock printargs
#define sys_pathconf printargs
#define sys_sigtimedwait printargs
#define sys_ulimit printargs
#define sys_ptrace printargs
#define sys_stty printargs
#define sys_lwp_info printargs
#define sys_priocntlsys printargs
#define sys_hrtsys printargs
#define sys_xenix printargs
#define sys_statfs printargs
#define sys_fstatfs printargs
#define sys_statvfs printargs
#define sys_fstatvfs printargs
#define sys_sigsendsys printargs
#define sys_gtty printargs
#define sys_vtrace printargs
#define sys_fpathconf printargs
#define sys_evsys printargs
#define sys_acct printargs
#define sys_exec printargs
#define sys_lwp_sema_post printargs
#define sys_nfssys printargs
#define sys_sigaltstack printargs
#define sys_uadmin printargs
#define sys_umount printargs
#define sys_modctl printargs
#define sys_acancel printargs
#define sys_async printargs
#define sys_evtrapret printargs
#define sys_lwp_create printargs
#define sys_lwp_exit printargs
#define sys_lwp_suspend printargs
#define sys_lwp_continue printargs
#define sys_lwp_kill printargs
#define sys_lwp_self printargs
#define sys_lwp_setprivate printargs
#define sys_lwp_getprivate printargs
#define sys_lwp_wait printargs
#define sys_lwp_mutex_unlock printargs
#define sys_lwp_mutex_lock printargs
#define sys_lwp_cond_wait printargs
#define sys_lwp_cond_signal printargs
#define sys_lwp_cond_broadcast printargs
#define sys_inst_sync printargs
#define sys_auditsys printargs
#define sys_processor_bind printargs
#define sys_processor_info printargs
#define sys_p_online printargs
#define sys_sigqueue printargs
#define sys_clock_gettime printargs
#define sys_clock_settime printargs
#define sys_clock_getres printargs
#define sys_nanosleep printargs
#define sys_timer_create printargs
#define sys_timer_delete printargs
#define sys_timer_settime printargs
#define sys_timer_gettime printargs
#define sys_timer_getoverrun printargs
#define sys_msgctl printargs
#define sys_msgget printargs
#define sys_msgrcv printargs
#define sys_msgsnd printargs
#define sys_shmat printargs
#define sys_shmctl printargs
#define sys_shmdt printargs
#define sys_shmget printargs
#define sys_semctl printargs
#define sys_semget printargs
#define sys_semop printargs
#define sys_olduname printargs
#define sys_ustat printargs
#define sys_fusers printargs
#define sys_sysfs1 printargs
#define sys_sysfs2 printargs
#define sys_sysfs3 printargs
#define sys_keyctl printargs
#define sys_secsys printargs
#define sys_filepriv printargs
#define sys_devstat printargs
#define sys_fdevstat printargs
#define sys_flvlfile printargs
#define sys_lvlfile printargs
#define sys_lvlequal printargs
#define sys_lvlproc printargs
#define sys_lvlipc printargs
#define sys_auditevt printargs
#define sys_auditctl printargs
#define sys_auditdmp printargs
#define sys_auditlog printargs
#define sys_auditbuf printargs
#define sys_lvldom printargs
#define sys_lvlvfs printargs
#define sys_mkmld printargs
#define sys_mldmode printargs
#define sys_secadvise printargs
#define sys_online printargs
#define sys_lwpinfo printargs
#define sys_lwpprivate printargs
#define sys_processor_exbind printargs
#define sys_prepblock printargs
#define sys_block printargs
#define sys_rdblock printargs
#define sys_unblock printargs
#define sys_cancelblock printargs
#define sys_lwpkill printargs
#define sys_modload printargs
#define sys_moduload printargs
#define sys_modpath printargs
#define sys_modstat printargs
#define sys_modadm printargs
#define sys_lwpsuspend printargs
#define sys_lwpcontinue printargs
#define sys_priocntllst printargs
#define sys_lwp_sema_trywait printargs
#define sys_xsetsockaddr printargs
#define sys_dshmsys printargs
#define sys_invlpg printargs
#define sys_migrate printargs
#define sys_kill3 printargs
#define sys_xbindresvport printargs
#define sys_lwp_sema_trywait printargs
#define sys_tsolsys printargs
#ifndef HAVE_SYS_ACL_H
#define sys_acl printargs
#define sys_facl printargs
#define sys_aclipc printargs
#endif
#define sys_install_utrap printargs
#define sys_signotify printargs
#define sys_schedctl printargs
#define sys_pset printargs
#define sys_resolvepath printargs
#define sys_signotifywait printargs
#define sys_lwp_sigredirect printargs
#define sys_lwp_alarm printargs
#define sys_rpcsys printargs
#define sys_sockconfig printargs
#define sys_ntp_gettime printargs
#define sys_ntp_adjtime printargs

/* like another call */
#define sys_lchown sys_chown
#define sys_setuid sys_close
#define sys_seteuid sys_close
#define sys_setgid sys_close
#define sys_setegid sys_close
#define sys_vhangup sys_close
#define sys_fdsync sys_close
#define sys_setreuid sys_dup2
#define sys_setregid sys_dup2
#define sys_sigfillset sys_sigpending
#define sys_vfork sys_fork
#define sys_ksigaction sys_sigaction
#define sys_BSDgetpgrp sys_getpgrp
#define sys_BSDsetpgrp sys_setpgrp
#define sys_waitsys sys_waitid
#define sys_sigset sys_signal
#define sys_sigrelse sys_sighold
#define sys_sigignore sys_sighold
#define sys_sigpause sys_sighold
#define sys_sleep sys_alarm
#define sys_fork1 sys_fork
#define sys_forkall sys_fork
#define sys_memcntl sys_mctl
#if UNIXWARE > 2
#define sys_rfork1 sys_rfork
#define sys_rforkall sys_rfork
#ifndef HAVE_SYS_NSCSYS_H
#define sys_ssisys printargs
#endif
#endif

/* aio */
#define sys_aionotify printargs
#define sys_aioinit printargs
#define sys_aiostart printargs
#define sys_aiolio printargs
#define sys_aiosuspend printargs
#define sys_aioerror printargs
#define sys_aioliowait printargs
#define sys_aioaread printargs
#define sys_aioawrite printargs
#define sys_aiolio64 printargs
#define sys_aiosuspend64 printargs
#define sys_aioerror64 printargs
#define sys_aioliowait64 printargs
#define sys_aioaread64 printargs
#define sys_aioaread64 printargs
#define sys_aioawrite64 printargs
#define sys_aiocancel64 printargs
#define sys_aiofsync printargs

/* the various 64-bit file stuff */
#if !_LFS64_LARGEFILE
/* we've implemented these */
#define sys_getdents64 printargs
#define sys_mmap64 printargs
#define sys_stat64 printargs
#define sys_lstat64 printargs
#define sys_fstat64 printargs
#define sys_setrlimit64 printargs
#define sys_getrlimit64 printargs
#define sys_pread64 printargs
#define sys_pwrite64 printargs
#define sys_ftruncate64 printargs
#define sys_truncate64 printargs
#define sys_lseek64 printargs
#endif

/* unimplemented 64-bit stuff */
#define sys_statvfs64 printargs
#define sys_fstatvfs64 printargs

/* like another call */
#define sys_creat64 sys_creat
#define sys_open64 sys_open
#define sys_llseek sys_lseek64

/* printargs does the right thing */
#define sys_sync printargs
#define sys_profil printargs
#define sys_yield printargs
#define sys_pause printargs
#define sys_sethostid printargs

/* subfunction entry points */
#define sys_pgrpsys printargs
#define sys_sigcall printargs
#define sys_msgsys printargs
#define sys_shmsys printargs
#define sys_semsys printargs
#define sys_utssys printargs
#define sys_sysfs printargs
#define sys_spcall printargs
#define sys_context printargs
#define sys_door printargs
#define sys_kaio printargs

#if DONE
#define sys_sigwait printargs
#define sys_mount printargs
#define sys_sysinfo printargs
#define sys_sysconfig printargs
#define sys_getpmsg printargs
#define sys_putpmsg printargs
#define sys_pread printargs
#define sys_pwrite printargs
#define sys_readv printargs
#define sys_writev printargs
#define sys_wait printargs
#define sys_waitid printargs
#define sys_sigsuspend printargs
#define sys_getpgrp printargs
#define sys_setpgrp printargs
#define sys_getsid printargs
#define sys_setsid printargs
#define sys_getpgid printargs
#define sys_setpgid printargs
#define sys_getcontext printargs
#define sys_setcontext printargs
#define sys_stime printargs
#define sys_time printargs
#define sys_nice printargs
#define sys_times printargs
#define sys_alarm printargs
#define sys_xstat printargs
#define sys_fxstat printargs
#define sys_lxstat printargs
#define sys_xmknod printargs
#define sys_exit printargs
#define sys_fork printargs
#define sys_read printargs
#define sys_write printargs
#define sys_open printargs
#define sys_close printargs
#define sys_creat printargs
#define sys_link printargs
#define sys_unlink printargs
#define sys_chdir printargs
#define sys_mknod printargs
#define sys_chmod printargs
#define sys_chown printargs
#define sys_brk printargs
#define sys_stat printargs
#define sys_lseek printargs
#define sys_getpid printargs
#define sys_getuid printargs
#define sys_fstat printargs
#define sys_utime printargs
#define sys_access printargs
#define sys_kill printargs
#define sys_dup printargs
#define sys_pipe printargs
#define sys_getgid printargs
#define sys_ioctl printargs
#define sys_umask printargs
#define sys_chroot printargs
#define sys_fcntl printargs
#define sys_rmdir printargs
#define sys_mkdir printargs
#define sys_getdents printargs
#define sys_getmsg printargs
#define sys_putmsg printargs
#define sys_poll printargs
#define sys_lstat printargs
#define sys_symlink printargs
#define sys_readlink printargs
#define sys_setgroups printargs
#define sys_getgroups printargs
#define sys_fchmod printargs
#define sys_fchown printargs
#define sys_sigprocmask printargs
#define sys_sigaction printargs
#define sys_sigpending printargs
#define sys_mincore printargs
#define sys_mmap printargs
#define sys_mprotect printargs
#define sys_munmap printargs
#define sys_vfork printargs
#define sys_fchdir printargs
#define sys_setrlimit printargs
#define sys_getrlimit printargs
#define sys_rename printargs
#define sys_uname printargs
#define sys_adjtime printargs
#define sys_fchroot printargs
#define sys_utimes printargs
#define sys_gettimeofday printargs
#define sys_getitimer printargs
#define sys_setitimer printargs
#define sys_settimeofday printargs
#endif
