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

/* Obsolete syscalls */
#define sys_otime	printargs
#define sys_osetuid	printargs
#define sys_ostime	printargs
#define sys_oalarm	printargs
#define sys_ofstat	printargs
#define sys_opause	printargs
#define sys_outime	printargs
#define sys_onice	printargs
#define sys_oftime	printargs
#define sys_osetpgrp	printargs
#define sys_otimes	printargs
#define sys_osetgid	printargs
#define sys_ossig	printargs
#define sys_owait3	printargs
#define sys_omsync	printargs
#define sys_ovadvise	printargs
#define sys_omadvise	printargs
#define sys_ovlimit	printargs
#define sys_owait	printargs
#define sys_ovtimes	printargs
#define sys_oldquota	printargs
#define sys_getdirentries	printargs

/* No interesting parameters or return values */
#define sys_vhangup	printargs
#define sys_sys_setsid	printargs
#define sys_errsys	printargs
#define sys_nosys	printargs

/* Don't know what to do with these */
#define sys_sstk	printargs
#define sys_profil	printargs
#define sys_vtrace	printargs
#define sys_async_daemon printargs
#define sys_nfs_getfh	printargs
#define sys_rtschedule	printargs
#define sys_auditsys	printargs
#define sys_rfssys	printargs
#define sys_vpixsys	printargs
#define sys_getdopt	printargs
#define sys_setdopt	printargs
#define sys_semsys	printargs
#define sys_msgsys	printargs
#define sys_shmsys	printargs
#define sys_semop	printargs

#if DONE
#define sys_rexit	printargs
#define sys_indir	printargs
#define sys_read	printargs
#define sys_write	printargs
#define sys_readv	printargs
#define sys_writev	printargs
#define sys_ioctl	printargs
#define sys_fcntl	printargs
#define sys_fstat	printargs
#define sys_stat	printargs
#define sys_lstat	printargs
#define sys_open	printargs
#define sys_creat	printargs
#define sys_close	printargs
#define sys_chdir	printargs
#define sys_fchdir	printargs
#define sys_mkdir	printargs
#define sys_rmdir	printargs
#define sys_chroot	printargs
#define sys_fchroot	printargs
#define sys_mknod	printargs
#define sys_link	printargs
#define sys_unlink	printargs
#define sys_chown	printargs
#define sys_fchown	printargs
#define sys_chmod	printargs
#define sys_fchmod	printargs
#define sys_utimes	printargs
#define sys_symlink	printargs
#define sys_readlink	printargs
#define sys_rename	printargs
#define sys_getdents	printargs
#define sys_truncate	printargs
#define sys_ftruncate	printargs
#define sys_access	printargs
#define sys_lseek	printargs
#define sys_socket	printargs
#define sys_bind	printargs
#define sys_connect	printargs
#define sys_listen	printargs
#define sys_accept	printargs
#define sys_shutdown	printargs
#define sys_send	printargs
#define sys_sendto	printargs
#define sys_sendmsg	printargs
#define sys_recv	printargs
#define sys_recvfrom	printargs
#define sys_recvmsg	printargs
#define sys_pipe	printargs
#define sys_socketpair	printargs
#define sys_setsockopt	printargs
#define sys_getsockopt	printargs
#define sys_getsockname	printargs
#define sys_getpeername	printargs
#define sys_gethostid	printargs
#define sys_gethostname	printargs
#define sys_sethostname	printargs
#define sys_getpid	printargs
#define sys_getdomainname	printargs
#define sys_setdomainname	printargs
#define sys_vfork	printargs
#define sys_fork	printargs
#define sys_getuid	printargs
#define sys_getgid	printargs
#define sys_setreuid	printargs
#define sys_setregid	printargs
#define sys_getgroups	printargs
#define sys_setgroups	printargs
#define sys_getpgrp	printargs
#define sys_setpgrp	printargs
#define sys_setpgid	printargs
#define sys_execv	printargs
#define sys_execve	printargs
#define sys_wait4	printargs
#define sys_uname	printargs
#define sys_ptrace	printargs
#define sys_brk		printargs
#define sys_sbrk	printargs
#define sys_mmap	printargs
#define sys_munmap	printargs
#define sys_mprotect	printargs
#define sys_mctl	printargs
#define sys_mincore	printargs
#define sys_sigvec	printargs
#define sys_sigblock	printargs
#define sys_sigsetmask	printargs
#define sys_sigpause	printargs
#define sys_sigstack	printargs
#define sys_sigcleanup	printargs
#define sys_sigpending	printargs
#define sys_kill	printargs
#define sys_killpg	printargs
#define sys_dup		printargs
#define sys_dup2	printargs
#define sys_getdtablesize	printargs
#define sys_select	printargs
#define sys_flock	printargs
#define sys_umask	printargs
#define sys_gettimeofday	printargs
#define sys_settimeofday	printargs
#define sys_getitimer	printargs
#define sys_setitimer	printargs
#define sys_adjtime	printargs
#define sys_setpriority	printargs
#define sys_getpriority	printargs
#define sys_getrusage	printargs
#define sys_getrlimit	printargs
#define sys_setrlimit	printargs
#define sys_quotactl	printargs
#define sys_sysacct	printargs
#define sys_reboot	printargs
#define sys_sync	printargs
#define sys_mount	printargs
#define sys_umount	printargs
#define sys_unmount	printargs
#define sys_swapon	printargs
#define sys_fsync	printargs
#define sys_exportfs	printargs
#define sys_nfs_svc	printargs
#define sys_statfs	printargs
#define sys_fstatfs	printargs
#define sys_ustat	printargs
#define sys_aioread	printargs
#define sys_aiowrite	printargs
#define sys_aiowait	printargs
#define sys_aiocancel	printargs
#define sys_getpagesize	printargs
#define sys_pathconf	printargs
#define sys_fpathconf	printargs
#define sys_sysconf	printargs
#define sys_getmsg	printargs
#define sys_putmsg	printargs
#define sys_poll	printargs
#endif
