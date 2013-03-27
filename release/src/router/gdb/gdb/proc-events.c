/* Machine-independent support for SVR4 /proc (process file system)

   Copyright (C) 1999, 2000, 2004, 2007 Free Software Foundation, Inc.

   Written by Michael Snyder at Cygnus Solutions.
   Based on work by Fred Fish, Stu Grossman, Geoff Noer, and others.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Pretty-print "events of interest".

   This module includes pretty-print routines for:
   * faults (hardware exceptions)
   * signals (software interrupts)
   * syscalls

   FIXME: At present, the syscall translation table must be
   initialized, which is not true of the other translation tables.  */

#include "defs.h"

#ifdef NEW_PROC_API
#define _STRUCTURED_PROC 1
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/procfs.h>
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_FAULT_H
#include <sys/fault.h>
#endif

/* Much of the information used in the /proc interface, particularly
   for printing status information, is kept as tables of structures of
   the following form.  These tables can be used to map numeric values
   to their symbolic names and to a string that describes their
   specific use.  */

struct trans
{
  int value;                    /* The numeric value.  */
  char *name;                   /* The equivalent symbolic value.  */
  char *desc;                   /* Short description of value.  */
};


/* Pretty print syscalls.  */

/* Ugh -- UnixWare and Solaris spell these differently!  */

#ifdef  SYS_lwpcreate
#define SYS_lwp_create	SYS_lwpcreate
#endif

#ifdef  SYS_lwpexit
#define SYS_lwp_exit SYS_lwpexit
#endif

#ifdef  SYS_lwpwait
#define SYS_lwp_wait SYS_lwpwait
#endif

#ifdef  SYS_lwpself
#define SYS_lwp_self SYS_lwpself
#endif

#ifdef  SYS_lwpinfo
#define SYS_lwp_info SYS_lwpinfo
#endif

#ifdef  SYS_lwpprivate
#define SYS_lwp_private SYS_lwpprivate
#endif

#ifdef  SYS_lwpkill
#define SYS_lwp_kill SYS_lwpkill
#endif

#ifdef  SYS_lwpsuspend
#define SYS_lwp_suspend SYS_lwpsuspend
#endif

#ifdef  SYS_lwpcontinue
#define SYS_lwp_continue SYS_lwpcontinue
#endif


/* Syscall translation table.  */

#define MAX_SYSCALLS 262	/* Pretty arbitrary.  */
static char *syscall_table[MAX_SYSCALLS];

void
init_syscall_table (void)
{
#ifdef SYS_BSD_getime
  syscall_table[SYS_BSD_getime] = "BSD_getime";
#endif
#ifdef SYS_BSDgetpgrp
  syscall_table[SYS_BSDgetpgrp] = "BSDgetpgrp";
#endif
#ifdef SYS_BSDsetpgrp
  syscall_table[SYS_BSDsetpgrp] = "BSDsetpgrp";
#endif
#ifdef SYS_acancel
  syscall_table[SYS_acancel] = "acancel";
#endif
#ifdef SYS_accept
  syscall_table[SYS_accept] = "accept";
#endif
#ifdef SYS_access
  syscall_table[SYS_access] = "access";
#endif
#ifdef SYS_acct
  syscall_table[SYS_acct] = "acct";
#endif
#ifdef SYS_acl
  syscall_table[SYS_acl] = "acl";
#endif
#ifdef SYS_aclipc
  syscall_table[SYS_aclipc] = "aclipc";
#endif
#ifdef SYS_adjtime
  syscall_table[SYS_adjtime] = "adjtime";
#endif
#ifdef SYS_afs_syscall
  syscall_table[SYS_afs_syscall] = "afs_syscall";
#endif
#ifdef SYS_alarm
  syscall_table[SYS_alarm] = "alarm";
#endif
#ifdef SYS_alt_plock
  syscall_table[SYS_alt_plock] = "alt_plock";
#endif
#ifdef SYS_alt_sigpending
  syscall_table[SYS_alt_sigpending] = "alt_sigpending";
#endif
#ifdef SYS_async
  syscall_table[SYS_async] = "async";
#endif
#ifdef SYS_async_daemon
  syscall_table[SYS_async_daemon] = "async_daemon";
#endif
#ifdef SYS_audcntl
  syscall_table[SYS_audcntl] = "audcntl";
#endif
#ifdef SYS_audgen
  syscall_table[SYS_audgen] = "audgen";
#endif
#ifdef SYS_auditbuf
  syscall_table[SYS_auditbuf] = "auditbuf";
#endif
#ifdef SYS_auditctl
  syscall_table[SYS_auditctl] = "auditctl";
#endif
#ifdef SYS_auditdmp
  syscall_table[SYS_auditdmp] = "auditdmp";
#endif
#ifdef SYS_auditevt
  syscall_table[SYS_auditevt] = "auditevt";
#endif
#ifdef SYS_auditlog
  syscall_table[SYS_auditlog] = "auditlog";
#endif
#ifdef SYS_auditsys
  syscall_table[SYS_auditsys] = "auditsys";
#endif
#ifdef SYS_bind
  syscall_table[SYS_bind] = "bind";
#endif
#ifdef SYS_block
  syscall_table[SYS_block] = "block";
#endif
#ifdef SYS_brk
  syscall_table[SYS_brk] = "brk";
#endif
#ifdef SYS_cachectl
  syscall_table[SYS_cachectl] = "cachectl";
#endif
#ifdef SYS_cacheflush
  syscall_table[SYS_cacheflush] = "cacheflush";
#endif
#ifdef SYS_cancelblock
  syscall_table[SYS_cancelblock] = "cancelblock";
#endif
#ifdef SYS_cg_bind
  syscall_table[SYS_cg_bind] = "cg_bind";
#endif
#ifdef SYS_cg_current
  syscall_table[SYS_cg_current] = "cg_current";
#endif
#ifdef SYS_cg_ids
  syscall_table[SYS_cg_ids] = "cg_ids";
#endif
#ifdef SYS_cg_info
  syscall_table[SYS_cg_info] = "cg_info";
#endif
#ifdef SYS_cg_memloc
  syscall_table[SYS_cg_memloc] = "cg_memloc";
#endif
#ifdef SYS_cg_processors
  syscall_table[SYS_cg_processors] = "cg_processors";
#endif
#ifdef SYS_chdir
  syscall_table[SYS_chdir] = "chdir";
#endif
#ifdef SYS_chflags
  syscall_table[SYS_chflags] = "chflags";
#endif
#ifdef SYS_chmod
  syscall_table[SYS_chmod] = "chmod";
#endif
#ifdef SYS_chown
  syscall_table[SYS_chown] = "chown";
#endif
#ifdef SYS_chroot
  syscall_table[SYS_chroot] = "chroot";
#endif
#ifdef SYS_clocal
  syscall_table[SYS_clocal] = "clocal";
#endif
#ifdef SYS_clock_getres
  syscall_table[SYS_clock_getres] = "clock_getres";
#endif
#ifdef SYS_clock_gettime
  syscall_table[SYS_clock_gettime] = "clock_gettime";
#endif
#ifdef SYS_clock_settime
  syscall_table[SYS_clock_settime] = "clock_settime";
#endif
#ifdef SYS_close
  syscall_table[SYS_close] = "close";
#endif
#ifdef SYS_connect
  syscall_table[SYS_connect] = "connect";
#endif
#ifdef SYS_context
  syscall_table[SYS_context] = "context";
#endif
#ifdef SYS_creat
  syscall_table[SYS_creat] = "creat";
#endif
#ifdef SYS_creat64
  syscall_table[SYS_creat64] = "creat64";
#endif
#ifdef SYS_devstat
  syscall_table[SYS_devstat] = "devstat";
#endif
#ifdef SYS_dmi
  syscall_table[SYS_dmi] = "dmi";
#endif
#ifdef SYS_door
  syscall_table[SYS_door] = "door";
#endif
#ifdef SYS_dshmsys
  syscall_table[SYS_dshmsys] = "dshmsys";
#endif
#ifdef SYS_dup
  syscall_table[SYS_dup] = "dup";
#endif
#ifdef SYS_dup2
  syscall_table[SYS_dup2] = "dup2";
#endif
#ifdef SYS_evsys
  syscall_table[SYS_evsys] = "evsys";
#endif
#ifdef SYS_evtrapret
  syscall_table[SYS_evtrapret] = "evtrapret";
#endif
#ifdef SYS_exec
  syscall_table[SYS_exec] = "exec";
#endif
#ifdef SYS_exec_with_loader
  syscall_table[SYS_exec_with_loader] = "exec_with_loader";
#endif
#ifdef SYS_execv
  syscall_table[SYS_execv] = "execv";
#endif
#ifdef SYS_execve
  syscall_table[SYS_execve] = "execve";
#endif
#ifdef SYS_exit
  syscall_table[SYS_exit] = "exit";
#endif
#ifdef SYS_exportfs
  syscall_table[SYS_exportfs] = "exportfs";
#endif
#ifdef SYS_facl
  syscall_table[SYS_facl] = "facl";
#endif
#ifdef SYS_fchdir
  syscall_table[SYS_fchdir] = "fchdir";
#endif
#ifdef SYS_fchflags
  syscall_table[SYS_fchflags] = "fchflags";
#endif
#ifdef SYS_fchmod
  syscall_table[SYS_fchmod] = "fchmod";
#endif
#ifdef SYS_fchown
  syscall_table[SYS_fchown] = "fchown";
#endif
#ifdef SYS_fchroot
  syscall_table[SYS_fchroot] = "fchroot";
#endif
#ifdef SYS_fcntl
  syscall_table[SYS_fcntl] = "fcntl";
#endif
#ifdef SYS_fdatasync
  syscall_table[SYS_fdatasync] = "fdatasync";
#endif
#ifdef SYS_fdevstat
  syscall_table[SYS_fdevstat] = "fdevstat";
#endif
#ifdef SYS_fdsync
  syscall_table[SYS_fdsync] = "fdsync";
#endif
#ifdef SYS_filepriv
  syscall_table[SYS_filepriv] = "filepriv";
#endif
#ifdef SYS_flock
  syscall_table[SYS_flock] = "flock";
#endif
#ifdef SYS_flvlfile
  syscall_table[SYS_flvlfile] = "flvlfile";
#endif
#ifdef SYS_fork
  syscall_table[SYS_fork] = "fork";
#endif
#ifdef SYS_fork1
  syscall_table[SYS_fork1] = "fork1";
#endif
#ifdef SYS_forkall
  syscall_table[SYS_forkall] = "forkall";
#endif
#ifdef SYS_fpathconf
  syscall_table[SYS_fpathconf] = "fpathconf";
#endif
#ifdef SYS_fstat
  syscall_table[SYS_fstat] = "fstat";
#endif
#ifdef SYS_fstat64
  syscall_table[SYS_fstat64] = "fstat64";
#endif
#ifdef SYS_fstatfs
  syscall_table[SYS_fstatfs] = "fstatfs";
#endif
#ifdef SYS_fstatvfs
  syscall_table[SYS_fstatvfs] = "fstatvfs";
#endif
#ifdef SYS_fstatvfs64
  syscall_table[SYS_fstatvfs64] = "fstatvfs64";
#endif
#ifdef SYS_fsync
  syscall_table[SYS_fsync] = "fsync";
#endif
#ifdef SYS_ftruncate
  syscall_table[SYS_ftruncate] = "ftruncate";
#endif
#ifdef SYS_ftruncate64
  syscall_table[SYS_ftruncate64] = "ftruncate64";
#endif
#ifdef SYS_fuser
  syscall_table[SYS_fuser] = "fuser";
#endif
#ifdef SYS_fxstat
  syscall_table[SYS_fxstat] = "fxstat";
#endif
#ifdef SYS_get_sysinfo
  syscall_table[SYS_get_sysinfo] = "get_sysinfo";
#endif
#ifdef SYS_getaddressconf
  syscall_table[SYS_getaddressconf] = "getaddressconf";
#endif
#ifdef SYS_getcontext
  syscall_table[SYS_getcontext] = "getcontext";
#endif
#ifdef SYS_getdents
  syscall_table[SYS_getdents] = "getdents";
#endif
#ifdef SYS_getdents64
  syscall_table[SYS_getdents64] = "getdents64";
#endif
#ifdef SYS_getdirentries
  syscall_table[SYS_getdirentries] = "getdirentries";
#endif
#ifdef SYS_getdomainname
  syscall_table[SYS_getdomainname] = "getdomainname";
#endif
#ifdef SYS_getdtablesize
  syscall_table[SYS_getdtablesize] = "getdtablesize";
#endif
#ifdef SYS_getfh
  syscall_table[SYS_getfh] = "getfh";
#endif
#ifdef SYS_getfsstat
  syscall_table[SYS_getfsstat] = "getfsstat";
#endif
#ifdef SYS_getgid
  syscall_table[SYS_getgid] = "getgid";
#endif
#ifdef SYS_getgroups
  syscall_table[SYS_getgroups] = "getgroups";
#endif
#ifdef SYS_gethostid
  syscall_table[SYS_gethostid] = "gethostid";
#endif
#ifdef SYS_gethostname
  syscall_table[SYS_gethostname] = "gethostname";
#endif
#ifdef SYS_getitimer
  syscall_table[SYS_getitimer] = "getitimer";
#endif
#ifdef SYS_getksym
  syscall_table[SYS_getksym] = "getksym";
#endif
#ifdef SYS_getlogin
  syscall_table[SYS_getlogin] = "getlogin";
#endif
#ifdef SYS_getmnt
  syscall_table[SYS_getmnt] = "getmnt";
#endif
#ifdef SYS_getmsg
  syscall_table[SYS_getmsg] = "getmsg";
#endif
#ifdef SYS_getpagesize
  syscall_table[SYS_getpagesize] = "getpagesize";
#endif
#ifdef SYS_getpeername
  syscall_table[SYS_getpeername] = "getpeername";
#endif
#ifdef SYS_getpgid
  syscall_table[SYS_getpgid] = "getpgid";
#endif
#ifdef SYS_getpgrp
  syscall_table[SYS_getpgrp] = "getpgrp";
#endif
#ifdef SYS_getpid
  syscall_table[SYS_getpid] = "getpid";
#endif
#ifdef SYS_getpmsg
  syscall_table[SYS_getpmsg] = "getpmsg";
#endif
#ifdef SYS_getpriority
  syscall_table[SYS_getpriority] = "getpriority";
#endif
#ifdef SYS_getrlimit
  syscall_table[SYS_getrlimit] = "getrlimit";
#endif
#ifdef SYS_getrlimit64
  syscall_table[SYS_getrlimit64] = "getrlimit64";
#endif
#ifdef SYS_getrusage
  syscall_table[SYS_getrusage] = "getrusage";
#endif
#ifdef SYS_getsid
  syscall_table[SYS_getsid] = "getsid";
#endif
#ifdef SYS_getsockname
  syscall_table[SYS_getsockname] = "getsockname";
#endif
#ifdef SYS_getsockopt
  syscall_table[SYS_getsockopt] = "getsockopt";
#endif
#ifdef SYS_gettimeofday
  syscall_table[SYS_gettimeofday] = "gettimeofday";
#endif
#ifdef SYS_getuid
  syscall_table[SYS_getuid] = "getuid";
#endif
#ifdef SYS_gtty
  syscall_table[SYS_gtty] = "gtty";
#endif
#ifdef SYS_hrtsys
  syscall_table[SYS_hrtsys] = "hrtsys";
#endif
#ifdef SYS_inst_sync
  syscall_table[SYS_inst_sync] = "inst_sync";
#endif
#ifdef SYS_install_utrap
  syscall_table[SYS_install_utrap] = "install_utrap";
#endif
#ifdef SYS_invlpg
  syscall_table[SYS_invlpg] = "invlpg";
#endif
#ifdef SYS_ioctl
  syscall_table[SYS_ioctl] = "ioctl";
#endif
#ifdef SYS_kaio
  syscall_table[SYS_kaio] = "kaio";
#endif
#ifdef SYS_keyctl
  syscall_table[SYS_keyctl] = "keyctl";
#endif
#ifdef SYS_kill
  syscall_table[SYS_kill] = "kill";
#endif
#ifdef SYS_killpg
  syscall_table[SYS_killpg] = "killpg";
#endif
#ifdef SYS_kloadcall
  syscall_table[SYS_kloadcall] = "kloadcall";
#endif
#ifdef SYS_kmodcall
  syscall_table[SYS_kmodcall] = "kmodcall";
#endif
#ifdef SYS_ksigaction
  syscall_table[SYS_ksigaction] = "ksigaction";
#endif
#ifdef SYS_ksigprocmask
  syscall_table[SYS_ksigprocmask] = "ksigprocmask";
#endif
#ifdef SYS_ksigqueue
  syscall_table[SYS_ksigqueue] = "ksigqueue";
#endif
#ifdef SYS_lchown
  syscall_table[SYS_lchown] = "lchown";
#endif
#ifdef SYS_link
  syscall_table[SYS_link] = "link";
#endif
#ifdef SYS_listen
  syscall_table[SYS_listen] = "listen";
#endif
#ifdef SYS_llseek
  syscall_table[SYS_llseek] = "llseek";
#endif
#ifdef SYS_lseek
  syscall_table[SYS_lseek] = "lseek";
#endif
#ifdef SYS_lseek64
  syscall_table[SYS_lseek64] = "lseek64";
#endif
#ifdef SYS_lstat
  syscall_table[SYS_lstat] = "lstat";
#endif
#ifdef SYS_lstat64
  syscall_table[SYS_lstat64] = "lstat64";
#endif
#ifdef SYS_lvldom
  syscall_table[SYS_lvldom] = "lvldom";
#endif
#ifdef SYS_lvlequal
  syscall_table[SYS_lvlequal] = "lvlequal";
#endif
#ifdef SYS_lvlfile
  syscall_table[SYS_lvlfile] = "lvlfile";
#endif
#ifdef SYS_lvlipc
  syscall_table[SYS_lvlipc] = "lvlipc";
#endif
#ifdef SYS_lvlproc
  syscall_table[SYS_lvlproc] = "lvlproc";
#endif
#ifdef SYS_lvlvfs
  syscall_table[SYS_lvlvfs] = "lvlvfs";
#endif
#ifdef SYS_lwp_alarm
  syscall_table[SYS_lwp_alarm] = "lwp_alarm";
#endif
#ifdef SYS_lwp_cond_broadcast
  syscall_table[SYS_lwp_cond_broadcast] = "lwp_cond_broadcast";
#endif
#ifdef SYS_lwp_cond_signal
  syscall_table[SYS_lwp_cond_signal] = "lwp_cond_signal";
#endif
#ifdef SYS_lwp_cond_wait
  syscall_table[SYS_lwp_cond_wait] = "lwp_cond_wait";
#endif
#ifdef SYS_lwp_continue
  syscall_table[SYS_lwp_continue] = "lwp_continue";
#endif
#ifdef SYS_lwp_create
  syscall_table[SYS_lwp_create] = "lwp_create";
#endif
#ifdef SYS_lwp_exit
  syscall_table[SYS_lwp_exit] = "lwp_exit";
#endif
#ifdef SYS_lwp_getprivate
  syscall_table[SYS_lwp_getprivate] = "lwp_getprivate";
#endif
#ifdef SYS_lwp_info
  syscall_table[SYS_lwp_info] = "lwp_info";
#endif
#ifdef SYS_lwp_kill
  syscall_table[SYS_lwp_kill] = "lwp_kill";
#endif
#ifdef SYS_lwp_mutex_init
  syscall_table[SYS_lwp_mutex_init] = "lwp_mutex_init";
#endif
#ifdef SYS_lwp_mutex_lock
  syscall_table[SYS_lwp_mutex_lock] = "lwp_mutex_lock";
#endif
#ifdef SYS_lwp_mutex_trylock
  syscall_table[SYS_lwp_mutex_trylock] = "lwp_mutex_trylock";
#endif
#ifdef SYS_lwp_mutex_unlock
  syscall_table[SYS_lwp_mutex_unlock] = "lwp_mutex_unlock";
#endif
#ifdef SYS_lwp_private
  syscall_table[SYS_lwp_private] = "lwp_private";
#endif
#ifdef SYS_lwp_self
  syscall_table[SYS_lwp_self] = "lwp_self";
#endif
#ifdef SYS_lwp_sema_post
  syscall_table[SYS_lwp_sema_post] = "lwp_sema_post";
#endif
#ifdef SYS_lwp_sema_trywait
  syscall_table[SYS_lwp_sema_trywait] = "lwp_sema_trywait";
#endif
#ifdef SYS_lwp_sema_wait
  syscall_table[SYS_lwp_sema_wait] = "lwp_sema_wait";
#endif
#ifdef SYS_lwp_setprivate
  syscall_table[SYS_lwp_setprivate] = "lwp_setprivate";
#endif
#ifdef SYS_lwp_sigredirect
  syscall_table[SYS_lwp_sigredirect] = "lwp_sigredirect";
#endif
#ifdef SYS_lwp_suspend
  syscall_table[SYS_lwp_suspend] = "lwp_suspend";
#endif
#ifdef SYS_lwp_wait
  syscall_table[SYS_lwp_wait] = "lwp_wait";
#endif
#ifdef SYS_lxstat
  syscall_table[SYS_lxstat] = "lxstat";
#endif
#ifdef SYS_madvise
  syscall_table[SYS_madvise] = "madvise";
#endif
#ifdef SYS_memcntl
  syscall_table[SYS_memcntl] = "memcntl";
#endif
#ifdef SYS_mincore
  syscall_table[SYS_mincore] = "mincore";
#endif
#ifdef SYS_mincore
  syscall_table[SYS_mincore] = "mincore";
#endif
#ifdef SYS_mkdir
  syscall_table[SYS_mkdir] = "mkdir";
#endif
#ifdef SYS_mkmld
  syscall_table[SYS_mkmld] = "mkmld";
#endif
#ifdef SYS_mknod
  syscall_table[SYS_mknod] = "mknod";
#endif
#ifdef SYS_mldmode
  syscall_table[SYS_mldmode] = "mldmode";
#endif
#ifdef SYS_mmap
  syscall_table[SYS_mmap] = "mmap";
#endif
#ifdef SYS_mmap64
  syscall_table[SYS_mmap64] = "mmap64";
#endif
#ifdef SYS_modadm
  syscall_table[SYS_modadm] = "modadm";
#endif
#ifdef SYS_modctl
  syscall_table[SYS_modctl] = "modctl";
#endif
#ifdef SYS_modload
  syscall_table[SYS_modload] = "modload";
#endif
#ifdef SYS_modpath
  syscall_table[SYS_modpath] = "modpath";
#endif
#ifdef SYS_modstat
  syscall_table[SYS_modstat] = "modstat";
#endif
#ifdef SYS_moduload
  syscall_table[SYS_moduload] = "moduload";
#endif
#ifdef SYS_mount
  syscall_table[SYS_mount] = "mount";
#endif
#ifdef SYS_mprotect
  syscall_table[SYS_mprotect] = "mprotect";
#endif
#ifdef SYS_mremap
  syscall_table[SYS_mremap] = "mremap";
#endif
#ifdef SYS_msfs_syscall
  syscall_table[SYS_msfs_syscall] = "msfs_syscall";
#endif
#ifdef SYS_msgctl
  syscall_table[SYS_msgctl] = "msgctl";
#endif
#ifdef SYS_msgget
  syscall_table[SYS_msgget] = "msgget";
#endif
#ifdef SYS_msgrcv
  syscall_table[SYS_msgrcv] = "msgrcv";
#endif
#ifdef SYS_msgsnd
  syscall_table[SYS_msgsnd] = "msgsnd";
#endif
#ifdef SYS_msgsys
  syscall_table[SYS_msgsys] = "msgsys";
#endif
#ifdef SYS_msleep
  syscall_table[SYS_msleep] = "msleep";
#endif
#ifdef SYS_msync
  syscall_table[SYS_msync] = "msync";
#endif
#ifdef SYS_munmap
  syscall_table[SYS_munmap] = "munmap";
#endif
#ifdef SYS_mvalid
  syscall_table[SYS_mvalid] = "mvalid";
#endif
#ifdef SYS_mwakeup
  syscall_table[SYS_mwakeup] = "mwakeup";
#endif
#ifdef SYS_naccept
  syscall_table[SYS_naccept] = "naccept";
#endif
#ifdef SYS_nanosleep
  syscall_table[SYS_nanosleep] = "nanosleep";
#endif
#ifdef SYS_nfssvc
  syscall_table[SYS_nfssvc] = "nfssvc";
#endif
#ifdef SYS_nfssys
  syscall_table[SYS_nfssys] = "nfssys";
#endif
#ifdef SYS_ngetpeername
  syscall_table[SYS_ngetpeername] = "ngetpeername";
#endif
#ifdef SYS_ngetsockname
  syscall_table[SYS_ngetsockname] = "ngetsockname";
#endif
#ifdef SYS_nice
  syscall_table[SYS_nice] = "nice";
#endif
#ifdef SYS_nrecvfrom
  syscall_table[SYS_nrecvfrom] = "nrecvfrom";
#endif
#ifdef SYS_nrecvmsg
  syscall_table[SYS_nrecvmsg] = "nrecvmsg";
#endif
#ifdef SYS_nsendmsg
  syscall_table[SYS_nsendmsg] = "nsendmsg";
#endif
#ifdef SYS_ntp_adjtime
  syscall_table[SYS_ntp_adjtime] = "ntp_adjtime";
#endif
#ifdef SYS_ntp_gettime
  syscall_table[SYS_ntp_gettime] = "ntp_gettime";
#endif
#ifdef SYS_nuname
  syscall_table[SYS_nuname] = "nuname";
#endif
#ifdef SYS_obreak
  syscall_table[SYS_obreak] = "obreak";
#endif
#ifdef SYS_old_accept
  syscall_table[SYS_old_accept] = "old_accept";
#endif
#ifdef SYS_old_fstat
  syscall_table[SYS_old_fstat] = "old_fstat";
#endif
#ifdef SYS_old_getpeername
  syscall_table[SYS_old_getpeername] = "old_getpeername";
#endif
#ifdef SYS_old_getpgrp
  syscall_table[SYS_old_getpgrp] = "old_getpgrp";
#endif
#ifdef SYS_old_getsockname
  syscall_table[SYS_old_getsockname] = "old_getsockname";
#endif
#ifdef SYS_old_killpg
  syscall_table[SYS_old_killpg] = "old_killpg";
#endif
#ifdef SYS_old_lstat
  syscall_table[SYS_old_lstat] = "old_lstat";
#endif
#ifdef SYS_old_recv
  syscall_table[SYS_old_recv] = "old_recv";
#endif
#ifdef SYS_old_recvfrom
  syscall_table[SYS_old_recvfrom] = "old_recvfrom";
#endif
#ifdef SYS_old_recvmsg
  syscall_table[SYS_old_recvmsg] = "old_recvmsg";
#endif
#ifdef SYS_old_send
  syscall_table[SYS_old_send] = "old_send";
#endif
#ifdef SYS_old_sendmsg
  syscall_table[SYS_old_sendmsg] = "old_sendmsg";
#endif
#ifdef SYS_old_sigblock
  syscall_table[SYS_old_sigblock] = "old_sigblock";
#endif
#ifdef SYS_old_sigsetmask
  syscall_table[SYS_old_sigsetmask] = "old_sigsetmask";
#endif
#ifdef SYS_old_sigvec
  syscall_table[SYS_old_sigvec] = "old_sigvec";
#endif
#ifdef SYS_old_stat
  syscall_table[SYS_old_stat] = "old_stat";
#endif
#ifdef SYS_old_vhangup
  syscall_table[SYS_old_vhangup] = "old_vhangup";
#endif
#ifdef SYS_old_wait
  syscall_table[SYS_old_wait] = "old_wait";
#endif
#ifdef SYS_oldquota
  syscall_table[SYS_oldquota] = "oldquota";
#endif
#ifdef SYS_online
  syscall_table[SYS_online] = "online";
#endif
#ifdef SYS_open
  syscall_table[SYS_open] = "open";
#endif
#ifdef SYS_open64
  syscall_table[SYS_open64] = "open64";
#endif
#ifdef SYS_ovadvise
  syscall_table[SYS_ovadvise] = "ovadvise";
#endif
#ifdef SYS_p_online
  syscall_table[SYS_p_online] = "p_online";
#endif
#ifdef SYS_pagelock
  syscall_table[SYS_pagelock] = "pagelock";
#endif
#ifdef SYS_pathconf
  syscall_table[SYS_pathconf] = "pathconf";
#endif
#ifdef SYS_pause
  syscall_table[SYS_pause] = "pause";
#endif
#ifdef SYS_pgrpsys
  syscall_table[SYS_pgrpsys] = "pgrpsys";
#endif
#ifdef SYS_pid_block
  syscall_table[SYS_pid_block] = "pid_block";
#endif
#ifdef SYS_pid_unblock
  syscall_table[SYS_pid_unblock] = "pid_unblock";
#endif
#ifdef SYS_pipe
  syscall_table[SYS_pipe] = "pipe";
#endif
#ifdef SYS_plock
  syscall_table[SYS_plock] = "plock";
#endif
#ifdef SYS_poll
  syscall_table[SYS_poll] = "poll";
#endif
#ifdef SYS_prctl
  syscall_table[SYS_prctl] = "prctl";
#endif
#ifdef SYS_pread
  syscall_table[SYS_pread] = "pread";
#endif
#ifdef SYS_pread64
  syscall_table[SYS_pread64] = "pread64";
#endif
#ifdef SYS_pread64
  syscall_table[SYS_pread64] = "pread64";
#endif
#ifdef SYS_prepblock
  syscall_table[SYS_prepblock] = "prepblock";
#endif
#ifdef SYS_priocntl
  syscall_table[SYS_priocntl] = "priocntl";
#endif
#ifdef SYS_priocntllst
  syscall_table[SYS_priocntllst] = "priocntllst";
#endif
#ifdef SYS_priocntlset
  syscall_table[SYS_priocntlset] = "priocntlset";
#endif
#ifdef SYS_priocntlsys
  syscall_table[SYS_priocntlsys] = "priocntlsys";
#endif
#ifdef SYS_procblk
  syscall_table[SYS_procblk] = "procblk";
#endif
#ifdef SYS_processor_bind
  syscall_table[SYS_processor_bind] = "processor_bind";
#endif
#ifdef SYS_processor_exbind
  syscall_table[SYS_processor_exbind] = "processor_exbind";
#endif
#ifdef SYS_processor_info
  syscall_table[SYS_processor_info] = "processor_info";
#endif
#ifdef SYS_procpriv
  syscall_table[SYS_procpriv] = "procpriv";
#endif
#ifdef SYS_profil
  syscall_table[SYS_profil] = "profil";
#endif
#ifdef SYS_proplist_syscall
  syscall_table[SYS_proplist_syscall] = "proplist_syscall";
#endif
#ifdef SYS_pset
  syscall_table[SYS_pset] = "pset";
#endif
#ifdef SYS_ptrace
  syscall_table[SYS_ptrace] = "ptrace";
#endif
#ifdef SYS_putmsg
  syscall_table[SYS_putmsg] = "putmsg";
#endif
#ifdef SYS_putpmsg
  syscall_table[SYS_putpmsg] = "putpmsg";
#endif
#ifdef SYS_pwrite
  syscall_table[SYS_pwrite] = "pwrite";
#endif
#ifdef SYS_pwrite64
  syscall_table[SYS_pwrite64] = "pwrite64";
#endif
#ifdef SYS_quotactl
  syscall_table[SYS_quotactl] = "quotactl";
#endif
#ifdef SYS_rdblock
  syscall_table[SYS_rdblock] = "rdblock";
#endif
#ifdef SYS_read
  syscall_table[SYS_read] = "read";
#endif
#ifdef SYS_readlink
  syscall_table[SYS_readlink] = "readlink";
#endif
#ifdef SYS_readv
  syscall_table[SYS_readv] = "readv";
#endif
#ifdef SYS_reboot
  syscall_table[SYS_reboot] = "reboot";
#endif
#ifdef SYS_recv
  syscall_table[SYS_recv] = "recv";
#endif
#ifdef SYS_recvfrom
  syscall_table[SYS_recvfrom] = "recvfrom";
#endif
#ifdef SYS_recvmsg
  syscall_table[SYS_recvmsg] = "recvmsg";
#endif
#ifdef SYS_rename
  syscall_table[SYS_rename] = "rename";
#endif
#ifdef SYS_resolvepath
  syscall_table[SYS_resolvepath] = "resolvepath";
#endif
#ifdef SYS_revoke
  syscall_table[SYS_revoke] = "revoke";
#endif
#ifdef SYS_rfsys
  syscall_table[SYS_rfsys] = "rfsys";
#endif
#ifdef SYS_rmdir
  syscall_table[SYS_rmdir] = "rmdir";
#endif
#ifdef SYS_rpcsys
  syscall_table[SYS_rpcsys] = "rpcsys";
#endif
#ifdef SYS_sbrk
  syscall_table[SYS_sbrk] = "sbrk";
#endif
#ifdef SYS_schedctl
  syscall_table[SYS_schedctl] = "schedctl";
#endif
#ifdef SYS_secadvise
  syscall_table[SYS_secadvise] = "secadvise";
#endif
#ifdef SYS_secsys
  syscall_table[SYS_secsys] = "secsys";
#endif
#ifdef SYS_security
  syscall_table[SYS_security] = "security";
#endif
#ifdef SYS_select
  syscall_table[SYS_select] = "select";
#endif
#ifdef SYS_semctl
  syscall_table[SYS_semctl] = "semctl";
#endif
#ifdef SYS_semget
  syscall_table[SYS_semget] = "semget";
#endif
#ifdef SYS_semop
  syscall_table[SYS_semop] = "semop";
#endif
#ifdef SYS_semsys
  syscall_table[SYS_semsys] = "semsys";
#endif
#ifdef SYS_send
  syscall_table[SYS_send] = "send";
#endif
#ifdef SYS_sendmsg
  syscall_table[SYS_sendmsg] = "sendmsg";
#endif
#ifdef SYS_sendto
  syscall_table[SYS_sendto] = "sendto";
#endif
#ifdef SYS_set_program_attributes
  syscall_table[SYS_set_program_attributes] = "set_program_attributes";
#endif
#ifdef SYS_set_speculative
  syscall_table[SYS_set_speculative] = "set_speculative";
#endif
#ifdef SYS_set_sysinfo
  syscall_table[SYS_set_sysinfo] = "set_sysinfo";
#endif
#ifdef SYS_setcontext
  syscall_table[SYS_setcontext] = "setcontext";
#endif
#ifdef SYS_setdomainname
  syscall_table[SYS_setdomainname] = "setdomainname";
#endif
#ifdef SYS_setegid
  syscall_table[SYS_setegid] = "setegid";
#endif
#ifdef SYS_seteuid
  syscall_table[SYS_seteuid] = "seteuid";
#endif
#ifdef SYS_setgid
  syscall_table[SYS_setgid] = "setgid";
#endif
#ifdef SYS_setgroups
  syscall_table[SYS_setgroups] = "setgroups";
#endif
#ifdef SYS_sethostid
  syscall_table[SYS_sethostid] = "sethostid";
#endif
#ifdef SYS_sethostname
  syscall_table[SYS_sethostname] = "sethostname";
#endif
#ifdef SYS_setitimer
  syscall_table[SYS_setitimer] = "setitimer";
#endif
#ifdef SYS_setlogin
  syscall_table[SYS_setlogin] = "setlogin";
#endif
#ifdef SYS_setpgid
  syscall_table[SYS_setpgid] = "setpgid";
#endif
#ifdef SYS_setpgrp
  syscall_table[SYS_setpgrp] = "setpgrp";
#endif
#ifdef SYS_setpriority
  syscall_table[SYS_setpriority] = "setpriority";
#endif
#ifdef SYS_setregid
  syscall_table[SYS_setregid] = "setregid";
#endif
#ifdef SYS_setreuid
  syscall_table[SYS_setreuid] = "setreuid";
#endif
#ifdef SYS_setrlimit
  syscall_table[SYS_setrlimit] = "setrlimit";
#endif
#ifdef SYS_setrlimit64
  syscall_table[SYS_setrlimit64] = "setrlimit64";
#endif
#ifdef SYS_setsid
  syscall_table[SYS_setsid] = "setsid";
#endif
#ifdef SYS_setsockopt
  syscall_table[SYS_setsockopt] = "setsockopt";
#endif
#ifdef SYS_settimeofday
  syscall_table[SYS_settimeofday] = "settimeofday";
#endif
#ifdef SYS_setuid
  syscall_table[SYS_setuid] = "setuid";
#endif
#ifdef SYS_sgi
  syscall_table[SYS_sgi] = "sgi";
#endif
#ifdef SYS_sgifastpath
  syscall_table[SYS_sgifastpath] = "sgifastpath";
#endif
#ifdef SYS_sgikopt
  syscall_table[SYS_sgikopt] = "sgikopt";
#endif
#ifdef SYS_sginap
  syscall_table[SYS_sginap] = "sginap";
#endif
#ifdef SYS_shmat
  syscall_table[SYS_shmat] = "shmat";
#endif
#ifdef SYS_shmctl
  syscall_table[SYS_shmctl] = "shmctl";
#endif
#ifdef SYS_shmdt
  syscall_table[SYS_shmdt] = "shmdt";
#endif
#ifdef SYS_shmget
  syscall_table[SYS_shmget] = "shmget";
#endif
#ifdef SYS_shmsys
  syscall_table[SYS_shmsys] = "shmsys";
#endif
#ifdef SYS_shutdown
  syscall_table[SYS_shutdown] = "shutdown";
#endif
#ifdef SYS_sigaction
  syscall_table[SYS_sigaction] = "sigaction";
#endif
#ifdef SYS_sigaltstack
  syscall_table[SYS_sigaltstack] = "sigaltstack";
#endif
#ifdef SYS_sigaltstack
  syscall_table[SYS_sigaltstack] = "sigaltstack";
#endif
#ifdef SYS_sigblock
  syscall_table[SYS_sigblock] = "sigblock";
#endif
#ifdef SYS_signal
  syscall_table[SYS_signal] = "signal";
#endif
#ifdef SYS_signotify
  syscall_table[SYS_signotify] = "signotify";
#endif
#ifdef SYS_signotifywait
  syscall_table[SYS_signotifywait] = "signotifywait";
#endif
#ifdef SYS_sigpending
  syscall_table[SYS_sigpending] = "sigpending";
#endif
#ifdef SYS_sigpoll
  syscall_table[SYS_sigpoll] = "sigpoll";
#endif
#ifdef SYS_sigprocmask
  syscall_table[SYS_sigprocmask] = "sigprocmask";
#endif
#ifdef SYS_sigqueue
  syscall_table[SYS_sigqueue] = "sigqueue";
#endif
#ifdef SYS_sigreturn
  syscall_table[SYS_sigreturn] = "sigreturn";
#endif
#ifdef SYS_sigsendset
  syscall_table[SYS_sigsendset] = "sigsendset";
#endif
#ifdef SYS_sigsendsys
  syscall_table[SYS_sigsendsys] = "sigsendsys";
#endif
#ifdef SYS_sigsetmask
  syscall_table[SYS_sigsetmask] = "sigsetmask";
#endif
#ifdef SYS_sigstack
  syscall_table[SYS_sigstack] = "sigstack";
#endif
#ifdef SYS_sigsuspend
  syscall_table[SYS_sigsuspend] = "sigsuspend";
#endif
#ifdef SYS_sigvec
  syscall_table[SYS_sigvec] = "sigvec";
#endif
#ifdef SYS_sigwait
  syscall_table[SYS_sigwait] = "sigwait";
#endif
#ifdef SYS_sigwaitprim
  syscall_table[SYS_sigwaitprim] = "sigwaitprim";
#endif
#ifdef SYS_sleep
  syscall_table[SYS_sleep] = "sleep";
#endif
#ifdef SYS_so_socket
  syscall_table[SYS_so_socket] = "so_socket";
#endif
#ifdef SYS_so_socketpair
  syscall_table[SYS_so_socketpair] = "so_socketpair";
#endif
#ifdef SYS_sockconfig
  syscall_table[SYS_sockconfig] = "sockconfig";
#endif
#ifdef SYS_socket
  syscall_table[SYS_socket] = "socket";
#endif
#ifdef SYS_socketpair
  syscall_table[SYS_socketpair] = "socketpair";
#endif
#ifdef SYS_sproc
  syscall_table[SYS_sproc] = "sproc";
#endif
#ifdef SYS_sprocsp
  syscall_table[SYS_sprocsp] = "sprocsp";
#endif
#ifdef SYS_sstk
  syscall_table[SYS_sstk] = "sstk";
#endif
#ifdef SYS_stat
  syscall_table[SYS_stat] = "stat";
#endif
#ifdef SYS_stat64
  syscall_table[SYS_stat64] = "stat64";
#endif
#ifdef SYS_statfs
  syscall_table[SYS_statfs] = "statfs";
#endif
#ifdef SYS_statvfs
  syscall_table[SYS_statvfs] = "statvfs";
#endif
#ifdef SYS_statvfs64
  syscall_table[SYS_statvfs64] = "statvfs64";
#endif
#ifdef SYS_stime
  syscall_table[SYS_stime] = "stime";
#endif
#ifdef SYS_stty
  syscall_table[SYS_stty] = "stty";
#endif
#ifdef SYS_subsys_info
  syscall_table[SYS_subsys_info] = "subsys_info";
#endif
#ifdef SYS_swapctl
  syscall_table[SYS_swapctl] = "swapctl";
#endif
#ifdef SYS_swapon
  syscall_table[SYS_swapon] = "swapon";
#endif
#ifdef SYS_symlink
  syscall_table[SYS_symlink] = "symlink";
#endif
#ifdef SYS_sync
  syscall_table[SYS_sync] = "sync";
#endif
#ifdef SYS_sys3b
  syscall_table[SYS_sys3b] = "sys3b";
#endif
#ifdef SYS_syscall
  syscall_table[SYS_syscall] = "syscall";
#endif
#ifdef SYS_sysconfig
  syscall_table[SYS_sysconfig] = "sysconfig";
#endif
#ifdef SYS_sysfs
  syscall_table[SYS_sysfs] = "sysfs";
#endif
#ifdef SYS_sysi86
  syscall_table[SYS_sysi86] = "sysi86";
#endif
#ifdef SYS_sysinfo
  syscall_table[SYS_sysinfo] = "sysinfo";
#endif
#ifdef SYS_sysmips
  syscall_table[SYS_sysmips] = "sysmips";
#endif
#ifdef SYS_syssun
  syscall_table[SYS_syssun] = "syssun";
#endif
#ifdef SYS_systeminfo
  syscall_table[SYS_systeminfo] = "systeminfo";
#endif
#ifdef SYS_table
  syscall_table[SYS_table] = "table";
#endif
#ifdef SYS_time
  syscall_table[SYS_time] = "time";
#endif
#ifdef SYS_timedwait
  syscall_table[SYS_timedwait] = "timedwait";
#endif
#ifdef SYS_timer_create
  syscall_table[SYS_timer_create] = "timer_create";
#endif
#ifdef SYS_timer_delete
  syscall_table[SYS_timer_delete] = "timer_delete";
#endif
#ifdef SYS_timer_getoverrun
  syscall_table[SYS_timer_getoverrun] = "timer_getoverrun";
#endif
#ifdef SYS_timer_gettime
  syscall_table[SYS_timer_gettime] = "timer_gettime";
#endif
#ifdef SYS_timer_settime
  syscall_table[SYS_timer_settime] = "timer_settime";
#endif
#ifdef SYS_times
  syscall_table[SYS_times] = "times";
#endif
#ifdef SYS_truncate
  syscall_table[SYS_truncate] = "truncate";
#endif
#ifdef SYS_truncate64
  syscall_table[SYS_truncate64] = "truncate64";
#endif
#ifdef SYS_tsolsys
  syscall_table[SYS_tsolsys] = "tsolsys";
#endif
#ifdef SYS_uadmin
  syscall_table[SYS_uadmin] = "uadmin";
#endif
#ifdef SYS_ulimit
  syscall_table[SYS_ulimit] = "ulimit";
#endif
#ifdef SYS_umask
  syscall_table[SYS_umask] = "umask";
#endif
#ifdef SYS_umount
  syscall_table[SYS_umount] = "umount";
#endif
#ifdef SYS_uname
  syscall_table[SYS_uname] = "uname";
#endif
#ifdef SYS_unblock
  syscall_table[SYS_unblock] = "unblock";
#endif
#ifdef SYS_unlink
  syscall_table[SYS_unlink] = "unlink";
#endif
#ifdef SYS_unmount
  syscall_table[SYS_unmount] = "unmount";
#endif
#ifdef SYS_usleep_thread
  syscall_table[SYS_usleep_thread] = "usleep_thread";
#endif
#ifdef SYS_uswitch
  syscall_table[SYS_uswitch] = "uswitch";
#endif
#ifdef SYS_utc_adjtime
  syscall_table[SYS_utc_adjtime] = "utc_adjtime";
#endif
#ifdef SYS_utc_gettime
  syscall_table[SYS_utc_gettime] = "utc_gettime";
#endif
#ifdef SYS_utime
  syscall_table[SYS_utime] = "utime";
#endif
#ifdef SYS_utimes
  syscall_table[SYS_utimes] = "utimes";
#endif
#ifdef SYS_utssys
  syscall_table[SYS_utssys] = "utssys";
#endif
#ifdef SYS_vfork
  syscall_table[SYS_vfork] = "vfork";
#endif
#ifdef SYS_vhangup
  syscall_table[SYS_vhangup] = "vhangup";
#endif
#ifdef SYS_vtrace
  syscall_table[SYS_vtrace] = "vtrace";
#endif
#ifdef SYS_wait
  syscall_table[SYS_wait] = "wait";
#endif
#ifdef SYS_waitid
  syscall_table[SYS_waitid] = "waitid";
#endif
#ifdef SYS_waitsys
  syscall_table[SYS_waitsys] = "waitsys";
#endif
#ifdef SYS_write
  syscall_table[SYS_write] = "write";
#endif
#ifdef SYS_writev
  syscall_table[SYS_writev] = "writev";
#endif
#ifdef SYS_xenix
  syscall_table[SYS_xenix] = "xenix";
#endif
#ifdef SYS_xmknod
  syscall_table[SYS_xmknod] = "xmknod";
#endif
#ifdef SYS_xstat
  syscall_table[SYS_xstat] = "xstat";
#endif
#ifdef SYS_yield
  syscall_table[SYS_yield] = "yield";
#endif
}

/* Prettyprint syscall NUM.  */

void
proc_prettyfprint_syscall (FILE *file, int num, int verbose)
{
  if (syscall_table[num])
    fprintf (file, "SYS_%s ", syscall_table[num]);
  else
    fprintf (file, "<Unknown syscall %d> ", num);
}

void
proc_prettyprint_syscall (int num, int verbose)
{
  proc_prettyfprint_syscall (stdout, num, verbose);
}

/* Prettyprint all syscalls in SYSSET.  */

void
proc_prettyfprint_syscalls (FILE *file, sysset_t *sysset, int verbose)
{
  int i;

  for (i = 0; i < MAX_SYSCALLS; i++)
    if (prismember (sysset, i))
      {
	proc_prettyfprint_syscall (file, i, verbose);
      }
  fprintf (file, "\n");
}

void
proc_prettyprint_syscalls (sysset_t *sysset, int verbose)
{
  proc_prettyfprint_syscalls (stdout, sysset, verbose);
}

/* Prettyprint signals.  */

/* Signal translation table.  */

static struct trans signal_table[] = 
{
  { 0,      "<no signal>", "no signal" }, 
#ifdef SIGHUP
  { SIGHUP, "SIGHUP", "Hangup" },
#endif
#ifdef SIGINT
  { SIGINT, "SIGINT", "Interrupt (rubout)" },
#endif
#ifdef SIGQUIT
  { SIGQUIT, "SIGQUIT", "Quit (ASCII FS)" },
#endif
#ifdef SIGILL
  { SIGILL, "SIGILL", "Illegal instruction" },	/* not reset when caught */
#endif
#ifdef SIGTRAP
  { SIGTRAP, "SIGTRAP", "Trace trap" },		/* not reset when caught */
#endif
#ifdef SIGABRT
  { SIGABRT, "SIGABRT", "used by abort()" },	/* replaces SIGIOT */
#endif
#ifdef SIGIOT
  { SIGIOT, "SIGIOT", "IOT instruction" },
#endif
#ifdef SIGEMT
  { SIGEMT, "SIGEMT", "EMT instruction" },
#endif
#ifdef SIGFPE
  { SIGFPE, "SIGFPE", "Floating point exception" },
#endif
#ifdef SIGKILL
  { SIGKILL, "SIGKILL", "Kill" },	/* Solaris: cannot be caught/ignored */
#endif
#ifdef SIGBUS
  { SIGBUS, "SIGBUS", "Bus error" },
#endif
#ifdef SIGSEGV
  { SIGSEGV, "SIGSEGV", "Segmentation violation" },
#endif
#ifdef SIGSYS
  { SIGSYS, "SIGSYS", "Bad argument to system call" },
#endif
#ifdef SIGPIPE
  { SIGPIPE, "SIGPIPE", "Write to pipe with no one to read it" },
#endif
#ifdef SIGALRM
  { SIGALRM, "SIGALRM", "Alarm clock" },
#endif
#ifdef SIGTERM
  { SIGTERM, "SIGTERM", "Software termination signal from kill" },
#endif
#ifdef SIGUSR1
  { SIGUSR1, "SIGUSR1", "User defined signal 1" },
#endif
#ifdef SIGUSR2
  { SIGUSR2, "SIGUSR2", "User defined signal 2" },
#endif
#ifdef SIGCHLD
  { SIGCHLD, "SIGCHLD", "Child status changed" },	/* Posix version */
#endif
#ifdef SIGCLD
  { SIGCLD, "SIGCLD", "Child status changed" },		/* Solaris version */
#endif
#ifdef SIGPWR
  { SIGPWR, "SIGPWR", "Power-fail restart" },
#endif
#ifdef SIGWINCH
  { SIGWINCH, "SIGWINCH", "Window size change" },
#endif
#ifdef SIGURG
  { SIGURG, "SIGURG", "Urgent socket condition" },
#endif
#ifdef SIGPOLL
  { SIGPOLL, "SIGPOLL", "Pollable event" },
#endif
#ifdef SIGIO
  { SIGIO, "SIGIO", "Socket I/O possible" },	/* alias for SIGPOLL */
#endif
#ifdef SIGSTOP
  { SIGSTOP, "SIGSTOP", "Stop, not from tty" },	/* cannot be caught or ignored */
#endif
#ifdef SIGTSTP
  { SIGTSTP, "SIGTSTP", "User stop from tty" },
#endif
#ifdef SIGCONT
  { SIGCONT, "SIGCONT", "Stopped process has been continued" },
#endif
#ifdef SIGTTIN
  { SIGTTIN, "SIGTTIN", "Background tty read attempted" },
#endif
#ifdef SIGTTOU
  { SIGTTOU, "SIGTTOU", "Background tty write attempted" },
#endif
#ifdef SIGVTALRM
  { SIGVTALRM, "SIGVTALRM", "Virtual timer expired" },
#endif
#ifdef SIGPROF
  { SIGPROF, "SIGPROF", "Profiling timer expired" },
#endif
#ifdef SIGXCPU
  { SIGXCPU, "SIGXCPU", "Exceeded CPU limit" },
#endif
#ifdef SIGXFSZ
  { SIGXFSZ, "SIGXFSZ", "Exceeded file size limit" },
#endif
#ifdef SIGWAITING
  { SIGWAITING, "SIGWAITING", "Process's LWPs are blocked" },
#endif
#ifdef SIGLWP
  { SIGLWP, "SIGLWP", "Used by thread library" },
#endif
#ifdef SIGFREEZE
  { SIGFREEZE, "SIGFREEZE", "Used by CPR" },
#endif
#ifdef SIGTHAW
  { SIGTHAW, "SIGTHAW", "Used by CPR" },
#endif
#ifdef SIGCANCEL
  { SIGCANCEL, "SIGCANCEL", "Used by libthread" },
#endif
#ifdef SIGLOST
  { SIGLOST, "SIGLOST", "Resource lost" },
#endif
#ifdef SIG32
  { SIG32, "SIG32", "Reserved for kernel usage (Irix)" },
#endif
#ifdef SIGPTINTR
  { SIGPTINTR, "SIGPTINTR", "Posix 1003.1b" },
#endif
#ifdef SIGTRESCHED
  { SIGTRESCHED, "SIGTRESCHED", "Posix 1003.1b" },
#endif
#ifdef SIGINFO
  { SIGINFO, "SIGINFO", "Information request" },
#endif
#ifdef SIGRESV
  { SIGRESV, "SIGRESV", "Reserved by Digital for future use" },
#endif
#ifdef SIGAIO
  { SIGAIO, "SIGAIO", "Asynchronous I/O signal" },
#endif

  /* FIXME: add real-time signals.  */
};

/* Prettyprint signal number SIGNO.  */

void
proc_prettyfprint_signal (FILE *file, int signo, int verbose)
{
  int i;

  for (i = 0; i < sizeof (signal_table) / sizeof (signal_table[0]); i++)
    if (signo == signal_table[i].value)
      {
	fprintf (file, "%s", signal_table[i].name);
	if (verbose)
	  fprintf (file, ": %s\n", signal_table[i].desc);
	else
	  fprintf (file, " ");
	return;
      }
  fprintf (file, "Unknown signal %d%c", signo, verbose ? '\n' : ' ');
}

void
proc_prettyprint_signal (int signo, int verbose)
{
  proc_prettyfprint_signal (stdout, signo, verbose);
}

/* Prettyprint all signals in SIGSET.  */

void
proc_prettyfprint_signalset (FILE *file, sigset_t *sigset, int verbose)
{
  int i;

  /* Loop over all signal numbers from 0 to NSIG, using them as the
     index to prismember.  The signal table had better not contain
     aliases, for if it does they will both be printed.  */

  for (i = 0; i < NSIG; i++)
    if (prismember (sigset, i))
      proc_prettyfprint_signal (file, i, verbose);

  if (!verbose)
    fprintf (file, "\n");
}

void
proc_prettyprint_signalset (sigset_t *sigset, int verbose)
{
  proc_prettyfprint_signalset (stdout, sigset, verbose);
}


/* Prettyprint faults.  */

/* Fault translation table.  */

static struct trans fault_table[] =
{
#ifdef FLTILL
  { FLTILL, "FLTILL", "Illegal instruction" },
#endif
#ifdef FLTPRIV
  { FLTPRIV, "FLTPRIV", "Privileged instruction" },
#endif
#ifdef FLTBPT
  { FLTBPT, "FLTBPT", "Breakpoint trap" },
#endif
#ifdef FLTTRACE
  { FLTTRACE, "FLTTRACE", "Trace trap" },
#endif
#ifdef FLTACCESS
  { FLTACCESS, "FLTACCESS", "Memory access fault" },
#endif
#ifdef FLTBOUNDS
  { FLTBOUNDS, "FLTBOUNDS", "Memory bounds violation" },
#endif
#ifdef FLTIOVF
  { FLTIOVF, "FLTIOVF", "Integer overflow" },
#endif
#ifdef FLTIZDIV
  { FLTIZDIV, "FLTIZDIV", "Integer zero divide" },
#endif
#ifdef FLTFPE
  { FLTFPE, "FLTFPE", "Floating-point exception" },
#endif
#ifdef FLTSTACK
  { FLTSTACK, "FLTSTACK", "Unrecoverable stack fault" },
#endif
#ifdef FLTPAGE
  { FLTPAGE, "FLTPAGE", "Recoverable page fault" },
#endif
#ifdef FLTPCINVAL
  { FLTPCINVAL, "FLTPCINVAL", "Invalid PC exception" },
#endif
#ifdef FLTWATCH
  { FLTWATCH, "FLTWATCH", "User watchpoint" },
#endif
#ifdef FLTKWATCH
  { FLTKWATCH, "FLTKWATCH", "Kernel watchpoint" },
#endif
#ifdef FLTSCWATCH
  { FLTSCWATCH, "FLTSCWATCH", "Hit a store conditional on a watched page" },
#endif
};

/* Work horse.  Accepts an index into the fault table, prints it
   pretty.  */

static void
prettyfprint_faulttable_entry (FILE *file, int i, int verbose)
{
  fprintf (file, "%s", fault_table[i].name);
  if (verbose)
    fprintf (file, ": %s\n", fault_table[i].desc);
  else
    fprintf (file, " ");
}

/* Prettyprint hardware fault number FAULTNO.  */

void
proc_prettyfprint_fault (FILE *file, int faultno, int verbose)
{
  int i;

  for (i = 0; i < ARRAY_SIZE (fault_table); i++)
    if (faultno == fault_table[i].value)
      {
	prettyfprint_faulttable_entry (file, i, verbose);
	return;
      }

  fprintf (file, "Unknown hardware fault %d%c", 
	   faultno, verbose ? '\n' : ' ');
}

void
proc_prettyprint_fault (int faultno, int verbose)
{
  proc_prettyfprint_fault (stdout, faultno, verbose);
}

/* Prettyprint all faults in FLTSET.  */

void
proc_prettyfprint_faultset (FILE *file, fltset_t *fltset, int verbose)
{
  int i;

  /* Loop through the fault table, using the value field as the index
     to prismember.  The fault table had better not contain aliases,
     for if it does they will both be printed.  */

  for (i = 0; i < ARRAY_SIZE (fault_table); i++)
    if (prismember (fltset, fault_table[i].value))
      prettyfprint_faulttable_entry (file, i, verbose);

  if (!verbose)
    fprintf (file, "\n");
}

void
proc_prettyprint_faultset (fltset_t *fltset, int verbose)
{
  proc_prettyfprint_faultset (stdout, fltset, verbose);
}

/* TODO: actions, holds...  */

void
proc_prettyprint_actionset (struct sigaction *actions, int verbose)
{
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_proc_events (void);

void
_initialize_proc_events (void)
{
  init_syscall_table ();
}
