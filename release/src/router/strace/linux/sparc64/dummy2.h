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

#define solaris_sysmp printargs
#define solaris_sginap printargs
#define solaris_sgikopt printargs
#define solaris_sysmips printargs
#define solaris_sigreturn printargs
#define solaris_recvmsg printargs
#define solaris_sendmsg printargs
#define solaris_nfssvc printargs
#define solaris_getfh printargs
#define solaris_async_daemon printargs
#define solaris_exportfs printargs
#define solaris_BSD_getime printargs
#define solaris_sproc printargs
#define solaris_procblk printargs
#define solaris_sprocsp printargs
#define solaris_msync printargs
#define solaris_madvise printargs
#define solaris_pagelock printargs
#define solaris_quotactl printargs
#define solaris_cacheflush printargs
#define solaris_cachectl printargs
#define solaris_nuname printargs
#define solaris_sigpoll printargs
#define solaris_swapctl printargs
#define solaris_sigstack printargs
#define solaris_sigsendset printargs
#define solaris_priocntl printargs
#define solaris_ksigqueue printargs
#define solaris_lwp_sema_wait printargs
#define solaris_memcntl printargs
#define solaris_syscall printargs
#define solaris_clocal printargs
#define solaris_syssun printargs
#define solaris_sysi86 printargs
#define solaris_sysmachine printargs
#define solaris_plock printargs
#define solaris_pathconf printargs
#define solaris_sigtimedwait printargs
#define solaris_ulimit printargs
#define solaris_ptrace printargs
#define solaris_stty printargs
#define solaris_lwp_info printargs
#define solaris_priocntlsys printargs
#define solaris_hrtsys printargs
#define solaris_xenix printargs
#define solaris_statfs printargs
#define solaris_fstatfs printargs
#define solaris_statvfs printargs
#define solaris_fstatvfs printargs
#define solaris_fork1 printargs
#define solaris_sigsendsys printargs
#define solaris_gtty printargs
#define solaris_vtrace printargs
#define solaris_fpathconf printargs
#define solaris_evsys printargs
#define solaris_acct printargs
#define solaris_exec printargs
#define solaris_lwp_sema_post printargs
#define solaris_nfssys printargs
#define solaris_sigaltstack printargs
#define solaris_uadmin printargs
#define solaris_umount printargs
#define solaris_modctl printargs
#define solaris_acancel printargs
#define solaris_async printargs
#define solaris_evtrapret printargs
#define solaris_lwp_create printargs
#define solaris_lwp_exit printargs
#define solaris_lwp_suspend printargs
#define solaris_lwp_continue printargs
#define solaris_lwp_kill printargs
#define solaris_lwp_self printargs
#define solaris_lwp_setprivate printargs
#define solaris_lwp_getprivate printargs
#define solaris_lwp_wait printargs
#define solaris_lwp_mutex_unlock printargs
#define solaris_lwp_mutex_lock printargs
#define solaris_lwp_cond_wait printargs
#define solaris_lwp_cond_signal printargs
#define solaris_lwp_cond_broadcast printargs
#define solaris_llseek printargs
#define solaris_inst_sync printargs
#define solaris_auditsys printargs
#define solaris_processor_bind printargs
#define solaris_processor_info printargs
#define solaris_p_online printargs
#define solaris_sigqueue printargs
#define solaris_clock_gettime printargs
#define solaris_clock_settime printargs
#define solaris_clock_getres printargs
#define solaris_nanosleep printargs
#define solaris_timer_create printargs
#define solaris_timer_delete printargs
#define solaris_timer_settime printargs
#define solaris_timer_gettime printargs
#define solaris_timer_getoverrun printargs
#define solaris_signal printargs
#define solaris_sigset printargs
#define solaris_sighold printargs
#define solaris_sigrelse printargs
#define solaris_sigignore printargs
#define solaris_sigpause printargs
#define solaris_msgctl printargs
#define solaris_msgget printargs
#define solaris_msgrcv printargs
#define solaris_msgsnd printargs
#define solaris_shmat printargs
#define solaris_shmctl printargs
#define solaris_shmdt printargs
#define solaris_shmget printargs
#define solaris_semctl printargs
#define solaris_semget printargs
#define solaris_semop printargs
#define solaris_olduname printargs
#define solaris_ustat printargs
#define solaris_fusers printargs
#define solaris_sysfs1 printargs
#define solaris_sysfs2 printargs
#define solaris_sysfs3 printargs

/* like another call */
#define solaris_lchown solaris_chown
#define solaris_setuid solaris_close
#define solaris_seteuid solaris_close
#define solaris_setgid solaris_close
#define solaris_setegid solaris_close
#define solaris_vhangup solaris_close
#define solaris_fdsync solaris_close
#define solaris_sigfillset solaris_sigpending
#define solaris_vfork solaris_fork
#define solaris_ksigaction solaris_sigaction
#define solaris_BSDgetpgrp solaris_getpgrp
#define solaris_BSDsetpgrp solaris_setpgrp
#define solaris_waitsys solaris_waitid

/* printargs does the right thing */
#define solaris_sync printargs
#define solaris_profil printargs
#define solaris_yield printargs
#define solaris_pause printargs
#define solaris_sethostid printargs

/* subfunction entry points */
#define solaris_pgrpsys printargs
#define solaris_sigcall printargs
#define solaris_msgsys printargs
#define solaris_shmsys printargs
#define solaris_semsys printargs
#define solaris_utssys printargs
#define solaris_sysfs printargs
#define solaris_spcall printargs
#define solaris_context printargs

/* same as linux */
#define solaris_exit sys_exit
#define solaris_fork sys_fork
#define solaris_read sys_read
#define solaris_write sys_write
#define solaris_close sys_close
#define solaris_creat sys_creat
#define solaris_link sys_link
#define solaris_unlink sys_unlink
#define solaris_chdir sys_chdir
#define solaris_time sys_time
#define solaris_chmod sys_chmod
#define solaris_lseek sys_lseek
#define solaris_stime sys_stime
#define solaris_alarm sys_alarm
#define solaris_utime sys_utime
#define solaris_access sys_access
#define solaris_nice sys_nice
#define solaris_dup sys_dup
#define solaris_pipe sys_pipe
#define solaris_times sys_times
#define solaris_execve sys_execve
#define solaris_umask sys_umask
#define solaris_chroot sys_chroot
#define solaris_rmdir sys_rmdir
#define solaris_mkdir sys_mkdir
#define solaris_getdents sys_getdents
#define solaris_poll sys_poll
#define solaris_symlink sys_symlink
#define solaris_readlink sys_readlink
#define solaris_setgroups sys_setgroups
#define solaris_getgroups sys_getgroups
#define solaris_fchmod sys_fchmod
#define solaris_fchown sys_fchown
#define solaris_mprotect sys_mprotect
#define solaris_munmap sys_munmap
#define solaris_readv sys_readv
#define solaris_writev sys_writev
#define solaris_chown sys_chown
#define solaris_rename sys_rename
#define solaris_gettimeofday sys_gettimeofday
#define solaris_getitimer sys_getitimer
#define solaris_setitimer sys_setitimer
#define solaris_brk sys_brk
#define solaris_mmap sys_mmap
#define solaris_getsid sys_getsid
#define solaris_setsid sys_setsid
#define solaris_getpgid sys_getpgid
#define solaris_setpgid sys_setpgid
#define solaris_getpgrp sys_getpgrp

/* These are handled according to current_personality */
#define solaris_xstat sys_xstat
#define solaris_fxstat sys_fxstat
#define solaris_lxstat sys_lxstat
#define solaris_xmknod sys_xmknod
#define solaris_stat sys_stat
#define solaris_fstat sys_fstat
#define solaris_lstat sys_lstat
#define solaris_pread sys_pread
#define solaris_pwrite sys_pwrite
#define solaris_ioctl sys_ioctl
#define solaris_mknod sys_mknod

/* To be done */
#define solaris_mount printargs
#define solaris_sysinfo printargs
#define solaris_sysconfig printargs
#define solaris_getpmsg printargs
#define solaris_putpmsg printargs
#define solaris_wait printargs
#define solaris_waitid printargs
#define solaris_sigsuspend printargs
#define solaris_setpgrp printargs
#define solaris_getcontext printargs
#define solaris_setcontext printargs
#define solaris_getpid printargs
#define solaris_getuid printargs
#define solaris_kill printargs
#define solaris_getgid printargs
#define solaris_fcntl printargs
#define solaris_getmsg printargs
#define solaris_putmsg printargs
#define solaris_sigprocmask printargs
#define solaris_sigaction printargs
#define solaris_sigpending printargs
#define solaris_mincore printargs
#define solaris_fchdir printargs
#define solaris_setrlimit printargs
#define solaris_getrlimit printargs
#define solaris_uname printargs
#define solaris_adjtime printargs
#define solaris_fchroot printargs
#define solaris_utimes printargs

#if DONE
#define solaris_open printargs
#endif
