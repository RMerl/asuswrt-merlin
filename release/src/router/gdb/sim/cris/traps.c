/* CRIS exception, interrupt, and trap (EIT) support
   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

This file is part of the GNU simulators.

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

#include "sim-main.h"
#include "sim-options.h"
#include "bfd.h"
/* FIXME: get rid of targ-vals.h usage everywhere else.  */

#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
/* For PATH_MAX, originally. */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* From ld/sysdep.h.  */
#ifdef PATH_MAX
# define SIM_PATHMAX PATH_MAX
#else
# ifdef MAXPATHLEN
#  define SIM_PATHMAX MAXPATHLEN
# else
#  define SIM_PATHMAX 1024
# endif
#endif

/* The verbatim values are from asm-cris/unistd.h.  */

#define TARGET_SYS_exit 1
#define TARGET_SYS_read 3
#define TARGET_SYS_write 4
#define TARGET_SYS_open 5
#define TARGET_SYS_close 6
#define TARGET_SYS_unlink 10
#define TARGET_SYS_time 13
#define TARGET_SYS_lseek 19
#define TARGET_SYS_getpid 20
#define TARGET_SYS_kill 37
#define TARGET_SYS_rename 38
#define TARGET_SYS_pipe 42
#define TARGET_SYS_brk 45
#define TARGET_SYS_ioctl 54
#define TARGET_SYS_fcntl 55
#define TARGET_SYS_getppid 64
#define TARGET_SYS_setrlimit 75
#define TARGET_SYS_gettimeofday	 78
#define TARGET_SYS_readlink 85
#define TARGET_SYS_munmap 91
#define TARGET_SYS_truncate 92
#define TARGET_SYS_ftruncate 93
#define TARGET_SYS_socketcall 102
#define TARGET_SYS_stat 106
#define TARGET_SYS_fstat 108
#define TARGET_SYS_wait4 114
#define TARGET_SYS_sigreturn 119
#define TARGET_SYS_clone 120
#define TARGET_SYS_uname 122
#define TARGET_SYS_mprotect 125
#define TARGET_SYS_llseek 140
#define TARGET_SYS__sysctl 149
#define TARGET_SYS_sched_setparam 154
#define TARGET_SYS_sched_getparam 155
#define TARGET_SYS_sched_setscheduler 156
#define TARGET_SYS_sched_getscheduler 157
#define TARGET_SYS_sched_yield 158
#define TARGET_SYS_sched_get_priority_max 159
#define TARGET_SYS_sched_get_priority_min 160
#define TARGET_SYS_mremap 163
#define TARGET_SYS_poll 168
#define TARGET_SYS_rt_sigaction 174
#define TARGET_SYS_rt_sigprocmask 175
#define TARGET_SYS_rt_sigsuspend 179
#define TARGET_SYS_getcwd 183
#define TARGET_SYS_ugetrlimit 191
#define TARGET_SYS_mmap2 192
#define TARGET_SYS_stat64 195
#define TARGET_SYS_lstat64 196
#define TARGET_SYS_fstat64 197
#define TARGET_SYS_geteuid32 201
#define TARGET_SYS_getuid32 199
#define TARGET_SYS_getegid32 202
#define TARGET_SYS_getgid32 200
#define TARGET_SYS_fcntl64 221

#define TARGET_PROT_READ	0x1
#define TARGET_PROT_WRITE	0x2
#define TARGET_PROT_EXEC	0x4
#define TARGET_PROT_NONE	0x0

#define TARGET_MAP_SHARED	0x01
#define TARGET_MAP_PRIVATE	0x02
#define TARGET_MAP_TYPE		0x0f
#define TARGET_MAP_FIXED	0x10
#define TARGET_MAP_ANONYMOUS	0x20

#define TARGET_CTL_KERN		1
#define TARGET_CTL_VM		2
#define TARGET_CTL_NET		3
#define TARGET_CTL_PROC		4
#define TARGET_CTL_FS		5
#define TARGET_CTL_DEBUG	6
#define TARGET_CTL_DEV		7
#define TARGET_CTL_BUS		8
#define TARGET_CTL_ABI		9

#define TARGET_CTL_KERN_VERSION	4

/* linux/mman.h */
#define TARGET_MREMAP_MAYMOVE  1
#define TARGET_MREMAP_FIXED    2

#define TARGET_TCGETS 0x5401

#define TARGET_UTSNAME "#38 Sun Apr 1 00:00:00 MET 2001"

/* Seconds since the above date + 10 minutes.  */
#define TARGET_EPOCH 986080200

/* Milliseconds since start of run.  We use the number of syscalls to
   avoid introducing noise in the execution time.  */
#define TARGET_TIME_MS(cpu) ((cpu)->syscalls)

/* Seconds as in time(2).  */
#define TARGET_TIME(cpu) (TARGET_EPOCH + TARGET_TIME_MS (cpu) / 1000)

#define TARGET_SCHED_OTHER 0

#define TARGET_RLIMIT_STACK 3
#define TARGET_RLIMIT_NOFILE 7

#define SIM_TARGET_MAX_THREADS 64
#define SIM_MAX_ALLOC_CHUNK (512*1024*1024)

/* From linux/sched.h.  */
#define TARGET_CSIGNAL 0x000000ff
#define TARGET_CLONE_VM	0x00000100
#define TARGET_CLONE_FS	0x00000200
#define TARGET_CLONE_FILES 0x00000400
#define TARGET_CLONE_SIGHAND 0x00000800
#define TARGET_CLONE_PID 0x00001000
#define TARGET_CLONE_PTRACE 0x00002000
#define TARGET_CLONE_VFORK 0x00004000
#define TARGET_CLONE_PARENT 0x00008000
#define TARGET_CLONE_THREAD 0x00010000
#define TARGET_CLONE_SIGNAL (TARGET_CLONE_SIGHAND | TARGET_CLONE_THREAD)

/* From asm-cris/poll.h.  */
#define TARGET_POLLIN 1

/* From asm-cris/signal.h.  */
#define TARGET_SIG_BLOCK 0
#define TARGET_SIG_UNBLOCK 1
#define TARGET_SIG_SETMASK 2

#define TARGET_SIG_DFL 0
#define TARGET_SIG_IGN 1
#define TARGET_SIG_ERR ((USI)-1)

#define TARGET_SIGHUP 1
#define TARGET_SIGINT 2
#define TARGET_SIGQUIT 3
#define TARGET_SIGILL 4
#define TARGET_SIGTRAP 5
#define TARGET_SIGABRT 6
#define TARGET_SIGIOT 6
#define TARGET_SIGBUS 7
#define TARGET_SIGFPE 8
#define TARGET_SIGKILL 9
#define TARGET_SIGUSR1 10
#define TARGET_SIGSEGV 11
#define TARGET_SIGUSR2 12
#define TARGET_SIGPIPE 13
#define TARGET_SIGALRM 14
#define TARGET_SIGTERM 15
#define TARGET_SIGSTKFLT 16
#define TARGET_SIGCHLD 17
#define TARGET_SIGCONT 18
#define TARGET_SIGSTOP 19
#define TARGET_SIGTSTP 20
#define TARGET_SIGTTIN 21
#define TARGET_SIGTTOU 22
#define TARGET_SIGURG 23
#define TARGET_SIGXCPU 24
#define TARGET_SIGXFSZ 25
#define TARGET_SIGVTALRM 26
#define TARGET_SIGPROF 27
#define TARGET_SIGWINCH 28
#define TARGET_SIGIO 29
#define TARGET_SIGPOLL SIGIO
/* Actually commented out in the kernel header.  */
#define TARGET_SIGLOST 29
#define TARGET_SIGPWR 30
#define TARGET_SIGSYS 31

/* From include/asm-cris/signal.h.  */
#define TARGET_SA_NOCLDSTOP 0x00000001
#define TARGET_SA_NOCLDWAIT 0x00000002 /* not supported yet */
#define TARGET_SA_SIGINFO 0x00000004
#define TARGET_SA_ONSTACK 0x08000000
#define TARGET_SA_RESTART 0x10000000
#define TARGET_SA_NODEFER 0x40000000
#define TARGET_SA_RESETHAND 0x80000000
#define TARGET_SA_INTERRUPT 0x20000000 /* dummy -- ignored */
#define TARGET_SA_RESTORER 0x04000000

/* From linux/wait.h.  */
#define TARGET_WNOHANG 1
#define TARGET_WUNTRACED 2
#define TARGET___WNOTHREAD 0x20000000
#define TARGET___WALL 0x40000000
#define TARGET___WCLONE 0x80000000

/* From linux/limits.h. */
#define TARGET_PIPE_BUF 4096

static const char stat_map[] =
"st_dev,2:space,10:space,4:st_mode,4:st_nlink,4:st_uid,4"
":st_gid,4:st_rdev,2:space,10:st_size,8:st_blksize,4:st_blocks,4"
":space,4:st_atime,4:space,4:st_mtime,4:space,4:st_ctime,4:space,4"
":st_ino,8";

static const CB_TARGET_DEFS_MAP syscall_map[] =
{
  { CB_SYS_open, TARGET_SYS_open },
  { CB_SYS_close, TARGET_SYS_close },
  { CB_SYS_read, TARGET_SYS_read },
  { CB_SYS_write, TARGET_SYS_write },
  { CB_SYS_lseek, TARGET_SYS_lseek },
  { CB_SYS_unlink, TARGET_SYS_unlink },
  { CB_SYS_getpid, TARGET_SYS_getpid },
  { CB_SYS_fstat, TARGET_SYS_fstat64 },
  { CB_SYS_lstat, TARGET_SYS_lstat64 },
  { CB_SYS_stat, TARGET_SYS_stat64 },
  { CB_SYS_pipe, TARGET_SYS_pipe },
  { CB_SYS_rename, TARGET_SYS_rename },
  { CB_SYS_truncate, TARGET_SYS_truncate },
  { CB_SYS_ftruncate, TARGET_SYS_ftruncate },
  { 0, -1 }
};

/* An older, 32-bit-only stat mapping.  */
static const char stat32_map[] =
"st_dev,2:space,2:st_ino,4:st_mode,2:st_nlink,2:st_uid,2"
":st_gid,2:st_rdev,2:space,2:st_size,4:st_blksize,4:st_blocks,4"
":st_atime,4:space,4:st_mtime,4:space,4:st_ctime,4:space,12";

/* Map for calls using the 32-bit struct stat.  Primarily used by the
   newlib Linux mapping.  */
static const CB_TARGET_DEFS_MAP syscall_stat32_map[] =
{
  { CB_SYS_fstat, TARGET_SYS_fstat },
  { CB_SYS_stat, TARGET_SYS_stat },
  { 0, -1 }
};

/* Giving the true value for the running sim process will lead to
   non-time-invariant behavior.  */
#define TARGET_PID 42

/* Unfortunately, we don't get this from cris.cpu at the moment, and if
   we did, we'd still don't get a register number with the "16" offset.  */
#define TARGET_SRP_REGNUM (16+11)

/* Extracted by applying
   awk '/^#define/ { printf "#ifdef %s\n  { %s, %s },\n#endif\n", $2, $2, $3;}'
   on .../include/asm/errno.h in a GNU/Linux/CRIS installation and
   adjusting the synonyms.  */

static const CB_TARGET_DEFS_MAP errno_map[] =
{
#ifdef EPERM
  { EPERM, 1 },
#endif
#ifdef ENOENT
  { ENOENT, 2 },
#endif
#ifdef ESRCH
  { ESRCH, 3 },
#endif
#ifdef EINTR
  { EINTR, 4 },
#endif
#ifdef EIO
  { EIO, 5 },
#endif
#ifdef ENXIO
  { ENXIO, 6 },
#endif
#ifdef E2BIG
  { E2BIG, 7 },
#endif
#ifdef ENOEXEC
  { ENOEXEC, 8 },
#endif
#ifdef EBADF
  { EBADF, 9 },
#endif
#ifdef ECHILD
  { ECHILD, 10 },
#endif
#ifdef EAGAIN
  { EAGAIN, 11 },
#endif
#ifdef ENOMEM
  { ENOMEM, 12 },
#endif
#ifdef EACCES
  { EACCES, 13 },
#endif
#ifdef EFAULT
  { EFAULT, 14 },
#endif
#ifdef ENOTBLK
  { ENOTBLK, 15 },
#endif
#ifdef EBUSY
  { EBUSY, 16 },
#endif
#ifdef EEXIST
  { EEXIST, 17 },
#endif
#ifdef EXDEV
  { EXDEV, 18 },
#endif
#ifdef ENODEV
  { ENODEV, 19 },
#endif
#ifdef ENOTDIR
  { ENOTDIR, 20 },
#endif
#ifdef EISDIR
  { EISDIR, 21 },
#endif
#ifdef EINVAL
  { EINVAL, 22 },
#endif
#ifdef ENFILE
  { ENFILE, 23 },
#endif
#ifdef EMFILE
  { EMFILE, 24 },
#endif
#ifdef ENOTTY
  { ENOTTY, 25 },
#endif
#ifdef ETXTBSY
  { ETXTBSY, 26 },
#endif
#ifdef EFBIG
  { EFBIG, 27 },
#endif
#ifdef ENOSPC
  { ENOSPC, 28 },
#endif
#ifdef ESPIPE
  { ESPIPE, 29 },
#endif
#ifdef EROFS
  { EROFS, 30 },
#endif
#ifdef EMLINK
  { EMLINK, 31 },
#endif
#ifdef EPIPE
  { EPIPE, 32 },
#endif
#ifdef EDOM
  { EDOM, 33 },
#endif
#ifdef ERANGE
  { ERANGE, 34 },
#endif
#ifdef EDEADLK
  { EDEADLK, 35 },
#endif
#ifdef ENAMETOOLONG
  { ENAMETOOLONG, 36 },
#endif
#ifdef ENOLCK
  { ENOLCK, 37 },
#endif
#ifdef ENOSYS
  { ENOSYS, 38 },
#endif
#ifdef ENOTEMPTY
  { ENOTEMPTY, 39 },
#endif
#ifdef ELOOP
  { ELOOP, 40 },
#endif
#ifdef EWOULDBLOCK
  { EWOULDBLOCK, 11 },
#endif
#ifdef ENOMSG
  { ENOMSG, 42 },
#endif
#ifdef EIDRM
  { EIDRM, 43 },
#endif
#ifdef ECHRNG
  { ECHRNG, 44 },
#endif
#ifdef EL2NSYNC
  { EL2NSYNC, 45 },
#endif
#ifdef EL3HLT
  { EL3HLT, 46 },
#endif
#ifdef EL3RST
  { EL3RST, 47 },
#endif
#ifdef ELNRNG
  { ELNRNG, 48 },
#endif
#ifdef EUNATCH
  { EUNATCH, 49 },
#endif
#ifdef ENOCSI
  { ENOCSI, 50 },
#endif
#ifdef EL2HLT
  { EL2HLT, 51 },
#endif
#ifdef EBADE
  { EBADE, 52 },
#endif
#ifdef EBADR
  { EBADR, 53 },
#endif
#ifdef EXFULL
  { EXFULL, 54 },
#endif
#ifdef ENOANO
  { ENOANO, 55 },
#endif
#ifdef EBADRQC
  { EBADRQC, 56 },
#endif
#ifdef EBADSLT
  { EBADSLT, 57 },
#endif
#ifdef EDEADLOCK
  { EDEADLOCK, 35 },
#endif
#ifdef EBFONT
  { EBFONT, 59 },
#endif
#ifdef ENOSTR
  { ENOSTR, 60 },
#endif
#ifdef ENODATA
  { ENODATA, 61 },
#endif
#ifdef ETIME
  { ETIME, 62 },
#endif
#ifdef ENOSR
  { ENOSR, 63 },
#endif
#ifdef ENONET
  { ENONET, 64 },
#endif
#ifdef ENOPKG
  { ENOPKG, 65 },
#endif
#ifdef EREMOTE
  { EREMOTE, 66 },
#endif
#ifdef ENOLINK
  { ENOLINK, 67 },
#endif
#ifdef EADV
  { EADV, 68 },
#endif
#ifdef ESRMNT
  { ESRMNT, 69 },
#endif
#ifdef ECOMM
  { ECOMM, 70 },
#endif
#ifdef EPROTO
  { EPROTO, 71 },
#endif
#ifdef EMULTIHOP
  { EMULTIHOP, 72 },
#endif
#ifdef EDOTDOT
  { EDOTDOT, 73 },
#endif
#ifdef EBADMSG
  { EBADMSG, 74 },
#endif
#ifdef EOVERFLOW
  { EOVERFLOW, 75 },
#endif
#ifdef ENOTUNIQ
  { ENOTUNIQ, 76 },
#endif
#ifdef EBADFD
  { EBADFD, 77 },
#endif
#ifdef EREMCHG
  { EREMCHG, 78 },
#endif
#ifdef ELIBACC
  { ELIBACC, 79 },
#endif
#ifdef ELIBBAD
  { ELIBBAD, 80 },
#endif
#ifdef ELIBSCN
  { ELIBSCN, 81 },
#endif
#ifdef ELIBMAX
  { ELIBMAX, 82 },
#endif
#ifdef ELIBEXEC
  { ELIBEXEC, 83 },
#endif
#ifdef EILSEQ
  { EILSEQ, 84 },
#endif
#ifdef ERESTART
  { ERESTART, 85 },
#endif
#ifdef ESTRPIPE
  { ESTRPIPE, 86 },
#endif
#ifdef EUSERS
  { EUSERS, 87 },
#endif
#ifdef ENOTSOCK
  { ENOTSOCK, 88 },
#endif
#ifdef EDESTADDRREQ
  { EDESTADDRREQ, 89 },
#endif
#ifdef EMSGSIZE
  { EMSGSIZE, 90 },
#endif
#ifdef EPROTOTYPE
  { EPROTOTYPE, 91 },
#endif
#ifdef ENOPROTOOPT
  { ENOPROTOOPT, 92 },
#endif
#ifdef EPROTONOSUPPORT
  { EPROTONOSUPPORT, 93 },
#endif
#ifdef ESOCKTNOSUPPORT
  { ESOCKTNOSUPPORT, 94 },
#endif
#ifdef EOPNOTSUPP
  { EOPNOTSUPP, 95 },
#endif
#ifdef EPFNOSUPPORT
  { EPFNOSUPPORT, 96 },
#endif
#ifdef EAFNOSUPPORT
  { EAFNOSUPPORT, 97 },
#endif
#ifdef EADDRINUSE
  { EADDRINUSE, 98 },
#endif
#ifdef EADDRNOTAVAIL
  { EADDRNOTAVAIL, 99 },
#endif
#ifdef ENETDOWN
  { ENETDOWN, 100 },
#endif
#ifdef ENETUNREACH
  { ENETUNREACH, 101 },
#endif
#ifdef ENETRESET
  { ENETRESET, 102 },
#endif
#ifdef ECONNABORTED
  { ECONNABORTED, 103 },
#endif
#ifdef ECONNRESET
  { ECONNRESET, 104 },
#endif
#ifdef ENOBUFS
  { ENOBUFS, 105 },
#endif
#ifdef EISCONN
  { EISCONN, 106 },
#endif
#ifdef ENOTCONN
  { ENOTCONN, 107 },
#endif
#ifdef ESHUTDOWN
  { ESHUTDOWN, 108 },
#endif
#ifdef ETOOMANYREFS
  { ETOOMANYREFS, 109 },
#endif
#ifdef ETIMEDOUT
  { ETIMEDOUT, 110 },
#endif
#ifdef ECONNREFUSED
  { ECONNREFUSED, 111 },
#endif
#ifdef EHOSTDOWN
  { EHOSTDOWN, 112 },
#endif
#ifdef EHOSTUNREACH
  { EHOSTUNREACH, 113 },
#endif
#ifdef EALREADY
  { EALREADY, 114 },
#endif
#ifdef EINPROGRESS
  { EINPROGRESS, 115 },
#endif
#ifdef ESTALE
  { ESTALE, 116 },
#endif
#ifdef EUCLEAN
  { EUCLEAN, 117 },
#endif
#ifdef ENOTNAM
  { ENOTNAM, 118 },
#endif
#ifdef ENAVAIL
  { ENAVAIL, 119 },
#endif
#ifdef EISNAM
  { EISNAM, 120 },
#endif
#ifdef EREMOTEIO
  { EREMOTEIO, 121 },
#endif
#ifdef EDQUOT
  { EDQUOT, 122 },
#endif
#ifdef ENOMEDIUM
  { ENOMEDIUM, 123 },
#endif
#ifdef EMEDIUMTYPE
  { EMEDIUMTYPE, 124 },
#endif
  { 0, -1 }
};

/* Extracted by applying
   perl -ne 'if ($_ =~ /^#define/) { split;
     printf "#ifdef $_[1]\n  { %s, 0x%x },\n#endif\n",
             $_[1], $_[2] =~ /^0/ ? oct($_[2]) : $_[2];}'
   on pertinent parts of .../include/asm/fcntl.h in a GNU/Linux/CRIS
   installation and removing synonyms and unnecessary items.  Don't
   forget the end-marker.  */

/* These we treat specially, as they're used in the fcntl F_GETFL
   syscall.  For consistency, open_map is also manually edited to use
   these macros.  */
#define TARGET_O_ACCMODE 0x3
#define TARGET_O_RDONLY 0x0
#define TARGET_O_WRONLY 0x1

static const CB_TARGET_DEFS_MAP open_map[] = {
#ifdef O_ACCMODE
  { O_ACCMODE, TARGET_O_ACCMODE },
#endif
#ifdef O_RDONLY
  { O_RDONLY, TARGET_O_RDONLY },
#endif
#ifdef O_WRONLY
  { O_WRONLY, TARGET_O_WRONLY },
#endif
#ifdef O_RDWR
  { O_RDWR, 0x2 },
#endif
#ifdef O_CREAT
  { O_CREAT, 0x40 },
#endif
#ifdef O_EXCL
  { O_EXCL, 0x80 },
#endif
#ifdef O_NOCTTY
  { O_NOCTTY, 0x100 },
#endif
#ifdef O_TRUNC
  { O_TRUNC, 0x200 },
#endif
#ifdef O_APPEND
  { O_APPEND, 0x400 },
#endif
#ifdef O_NONBLOCK
  { O_NONBLOCK, 0x800 },
#endif
#ifdef O_NDELAY
  { O_NDELAY, 0x0 },
#endif
#ifdef O_SYNC
  { O_SYNC, 0x1000 },
#endif
#ifdef FASYNC
  { FASYNC, 0x2000 },
#endif
#ifdef O_DIRECT
  { O_DIRECT, 0x4000 },
#endif
#ifdef O_LARGEFILE
  { O_LARGEFILE, 0x8000 },
#endif
#ifdef O_DIRECTORY
  { O_DIRECTORY, 0x10000 },
#endif
#ifdef O_NOFOLLOW
  { O_NOFOLLOW, 0x20000 },
#endif
  { -1, -1 }
};

/* Needed for the cris_pipe_nonempty and cris_pipe_empty syscalls.  */
static SIM_CPU *current_cpu_for_cb_callback;

static int syscall_read_mem (host_callback *, struct cb_syscall *,
			     unsigned long, char *, int);
static int syscall_write_mem (host_callback *, struct cb_syscall *,
			      unsigned long, const char *, int);
static USI create_map (SIM_DESC, struct cris_sim_mmapped_page **,
		       USI addr, USI len);
static USI unmap_pages (SIM_DESC, struct cris_sim_mmapped_page **,
		       USI addr, USI len);
static USI is_mapped (SIM_DESC, struct cris_sim_mmapped_page **,
		       USI addr, USI len);
static void dump_statistics (SIM_CPU *current_cpu);
static void make_first_thread (SIM_CPU *current_cpu);

/* Read/write functions for system call interface.  */

static int
syscall_read_mem (host_callback *cb ATTRIBUTE_UNUSED,
		  struct cb_syscall *sc,
		  unsigned long taddr, char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_read_buffer (sd, cpu, read_map, buf, taddr, bytes);
}

static int
syscall_write_mem (host_callback *cb ATTRIBUTE_UNUSED,
		   struct cb_syscall *sc,
		   unsigned long taddr, const char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_write_buffer (sd, cpu, write_map, buf, taddr, bytes);
}

/* When we risk running self-modified code (as in trampolines), this is
   called from special-case insns.  The silicon CRIS CPU:s have enough
   cache snooping implemented making this a simulator-only issue.  Tests:
   gcc.c-torture/execute/931002-1.c execution, -O3 -g
   gcc.c-torture/execute/931002-1.c execution, -O3 -fomit-frame-pointer.  */

void
cris_flush_simulator_decode_cache (SIM_CPU *current_cpu,
				   USI pc ATTRIBUTE_UNUSED)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

#if WITH_SCACHE
  if (USING_SCACHE_P (sd))
    scache_flush_cpu (current_cpu);
#endif
}

/* Output statistics at the end of a run.  */
static void
dump_statistics (SIM_CPU *current_cpu)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  CRIS_MISC_PROFILE *profp
    = CPU_CRIS_MISC_PROFILE (current_cpu);
  unsigned64 total = profp->basic_cycle_count;
  const char *textmsg = "Basic clock cycles, total @: %llu\n";

  /* The --cris-stats={basic|unaligned|schedulable|all} counts affect
     what's included in the "total" count only.  */
  switch (CPU_CRIS_MISC_PROFILE (current_cpu)->flags
	  & FLAG_CRIS_MISC_PROFILE_ALL)
    {
    case FLAG_CRIS_MISC_PROFILE_SIMPLE:
      break;

    case (FLAG_CRIS_MISC_PROFILE_UNALIGNED | FLAG_CRIS_MISC_PROFILE_SIMPLE):
      textmsg
	= "Clock cycles including stall cycles for unaligned accesses @: %llu\n";
      total += profp->unaligned_mem_dword_count;
      break;

    case (FLAG_CRIS_MISC_PROFILE_SCHEDULABLE | FLAG_CRIS_MISC_PROFILE_SIMPLE):
      textmsg = "Schedulable clock cycles, total @: %llu\n";
      total
	+= (profp->memsrc_stall_count
	    + profp->memraw_stall_count
	    + profp->movemsrc_stall_count
	    + profp->movemdst_stall_count
	    + profp->mulsrc_stall_count
	    + profp->jumpsrc_stall_count
	    + profp->unaligned_mem_dword_count);
      break;

    case FLAG_CRIS_MISC_PROFILE_ALL:
      textmsg = "All accounted clock cycles, total @: %llu\n";
      total
	+= (profp->memsrc_stall_count
	    + profp->memraw_stall_count
	    + profp->movemsrc_stall_count
	    + profp->movemdst_stall_count
	    + profp->movemaddr_stall_count
	    + profp->mulsrc_stall_count
	    + profp->jumpsrc_stall_count
	    + profp->branch_stall_count
	    + profp->jumptarget_stall_count
	    + profp->unaligned_mem_dword_count);
      break;

    default:
      abort ();

      sim_io_eprintf (sd,
		      "Internal inconsistency at %s:%d",
		      __FILE__, __LINE__);
      sim_engine_halt (sd, current_cpu, NULL, 0,
		       sim_stopped, SIM_SIGILL);
    }

  /* Historically, these messages have gone to stderr, so we'll keep it
     that way.  It's also easier to then tell it from normal program
     output.  FIXME: Add redirect option like "run -e file".  */
  sim_io_eprintf (sd, textmsg, total);

  /* For v32, unaligned_mem_dword_count should always be 0.  For
     v10, memsrc_stall_count should always be 0.  */
  sim_io_eprintf (sd, "Memory source stall cycles: %llu\n",
		  (unsigned long long) (profp->memsrc_stall_count
					+ profp->unaligned_mem_dword_count));
  sim_io_eprintf (sd, "Memory read-after-write stall cycles: %llu\n",
		  (unsigned long long) profp->memraw_stall_count);
  sim_io_eprintf (sd, "Movem source stall cycles: %llu\n",
		  (unsigned long long) profp->movemsrc_stall_count);
  sim_io_eprintf (sd, "Movem destination stall cycles: %llu\n",
		  (unsigned long long) profp->movemdst_stall_count);
  sim_io_eprintf (sd, "Movem address stall cycles: %llu\n",
		  (unsigned long long) profp->movemaddr_stall_count);
  sim_io_eprintf (sd, "Multiplication source stall cycles: %llu\n",
		  (unsigned long long) profp->mulsrc_stall_count);
  sim_io_eprintf (sd, "Jump source stall cycles: %llu\n",
		  (unsigned long long) profp->jumpsrc_stall_count);
  sim_io_eprintf (sd, "Branch misprediction stall cycles: %llu\n",
		  (unsigned long long) profp->branch_stall_count);
  sim_io_eprintf (sd, "Jump target stall cycles: %llu\n",
		  (unsigned long long) profp->jumptarget_stall_count);
}

/* Check whether any part of [addr .. addr + len - 1] is already mapped.
   Return 1 if a overlap detected, 0 otherwise.  */

static USI
is_mapped (SIM_DESC sd ATTRIBUTE_UNUSED,
	   struct cris_sim_mmapped_page **rootp,
	   USI addr, USI len)
{
  struct cris_sim_mmapped_page *mapp;

  if (len == 0 || (len & 8191))
    abort ();

  /* Iterate over the reverse-address sorted pages until we find a page in
     or lower than the checked area.  */
  for (mapp = *rootp; mapp != NULL && mapp->addr >= addr; mapp = mapp->prev)
    if (mapp->addr < addr + len && mapp->addr >= addr)
      return 1;

  return 0;
}

/* Create mmapped memory.  */

static USI
create_map (SIM_DESC sd, struct cris_sim_mmapped_page **rootp, USI addr,
	    USI len)
{
  struct cris_sim_mmapped_page *mapp;
  struct cris_sim_mmapped_page **higher_prevp = rootp;
  USI new_addr = 0x40000000;

  if (addr != 0)
    new_addr = addr;
  else if (*rootp)
    new_addr = rootp[0]->addr + 8192;

  if (len != 8192)
    {
      USI page_addr;

      if (len & 8191)
	/* Which is better: return an error for this, or just round it up?  */
	abort ();

      /* Do a recursive call for each page in the request.  */
      for (page_addr = new_addr; len != 0; page_addr += 8192, len -= 8192)
	if (create_map (sd, rootp, page_addr, 8192) >= (USI) -8191)
	  abort ();

      return new_addr;
    }

  for (mapp = *rootp;
       mapp != NULL && mapp->addr > new_addr;
       mapp = mapp->prev)
    higher_prevp = &mapp->prev;

  /* Allocate the new page, on the next higher page from the last one
     allocated, and link in the new descriptor before previous ones.  */
  mapp = malloc (sizeof (*mapp));

  if (mapp == NULL)
    return (USI) -ENOMEM;

  sim_core_attach (sd, NULL, 0, access_read_write_exec, 0,
		   new_addr, len,
		   0, NULL, NULL);

  mapp->addr = new_addr;
  mapp->prev = *higher_prevp;
  *higher_prevp = mapp;

  return new_addr;
}

/* Unmap one or more pages.  */

static USI
unmap_pages (SIM_DESC sd, struct cris_sim_mmapped_page **rootp, USI addr,
	    USI len)
{
  struct cris_sim_mmapped_page *mapp;
  struct cris_sim_mmapped_page **higher_prevp = rootp;

  if (len != 8192)
    {
      USI page_addr;

      if (len & 8191)
	/* Which is better: return an error for this, or just round it up?  */
	abort ();

      /* Loop backwards to make each call is O(1) over the number of pages
	 allocated, if we're unmapping from the high end of the pages.  */
      for (page_addr = addr + len - 8192;
	   page_addr >= addr;
	   page_addr -= 8192)
	if (unmap_pages (sd, rootp, page_addr, 8192) != 0)
	  abort ();

      return 0;
    }

  for (mapp = *rootp; mapp != NULL && mapp->addr > addr; mapp = mapp->prev)
    higher_prevp = &mapp->prev;

  if (mapp == NULL || mapp->addr != addr)
    return EINVAL;

  *higher_prevp = mapp->prev;
  sim_core_detach (sd, NULL, 0, 0, addr);
  free (mapp);
  return 0;
}

/* The semantic code invokes this for illegal (unrecognized) instructions.  */

SEM_PC
sim_engine_invalid_insn (SIM_CPU *current_cpu, IADDR cia, SEM_PC vpc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

  sim_engine_halt (sd, current_cpu, NULL, cia, sim_stopped, SIM_SIGILL);
  return vpc;
}

/* Handlers from the CGEN description that should not be called.  */

USI
cris_bmod_handler (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
		   UINT srcreg ATTRIBUTE_UNUSED,
		   USI dstreg ATTRIBUTE_UNUSED)
{
  abort ();
}

void
h_supr_set_handler (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
		    UINT index ATTRIBUTE_UNUSED,
		    USI page ATTRIBUTE_UNUSED,
		    USI newval ATTRIBUTE_UNUSED)
{
  abort ();
}

USI
h_supr_get_handler (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
		    UINT index ATTRIBUTE_UNUSED,
		    USI page ATTRIBUTE_UNUSED)
{
  abort ();
}

/* Swap one context for another.  */

static void
schedule (SIM_CPU *current_cpu, int next)
{
  /* Need to mark context-switches in the trace output.  */
  if ((CPU_CRIS_MISC_PROFILE (current_cpu)->flags
       & FLAG_CRIS_MISC_PROFILE_XSIM_TRACE))
    cris_trace_printf (CPU_STATE (current_cpu), current_cpu,
		       "\t#:%d\n", next);

  /* Copy the current context (if there is one) to its slot.  */
  if (current_cpu->thread_data[current_cpu->threadno].cpu_context)
    memcpy (current_cpu->thread_data[current_cpu->threadno].cpu_context,
	    &current_cpu->cpu_data_placeholder,
	    current_cpu->thread_cpu_data_size);

  /* Copy the new context from its slot.  */
  memcpy (&current_cpu->cpu_data_placeholder,
	  current_cpu->thread_data[next].cpu_context,
	  current_cpu->thread_cpu_data_size);

  /* Update needed stuff to indicate the new context.  */
  current_cpu->threadno = next;

  /* Handle pending signals.  */
  if (current_cpu->thread_data[next].sigpending
      /* We don't run nested signal handlers.  This means that pause(2)
	 and sigsuspend(2) do not work in sighandlers, but that
	 shouldn't be too hard a restriction.  It also greatly
	 simplifies the code.  */
      && current_cpu->thread_data[next].cpu_context_atsignal == NULL)
  {
    int sig;

    /* See if there's really a pending, non-blocked handler.  We don't
       queue signals, so just use the first one in ascending order.  */
    for (sig = 0; sig < 64; sig++)
      if (current_cpu->thread_data[next].sigdata[sig].pending
	  && !current_cpu->thread_data[next].sigdata[sig].blocked)
      {
	bfd_byte regbuf[4];
	USI sp;
	int i;
	USI blocked;
	USI pc = sim_pc_get (current_cpu);

	/* It's simpler to save the CPU context inside the simulator
	   than on the stack.  */
	current_cpu->thread_data[next].cpu_context_atsignal
	  = (*current_cpu
	     ->make_thread_cpu_data) (current_cpu,
				      current_cpu->thread_data[next]
				      .cpu_context);

	(*CPU_REG_FETCH (current_cpu)) (current_cpu, H_GR_SP, regbuf, 4);
	sp = bfd_getl32 (regbuf);

	/* Make sure we have an aligned stack.  */
	sp &= ~3;

	/* Make room for the signal frame, aligned.  FIXME: Check that
	   the memory exists, map it in if absent.  (BTW, should also
	   implement on-access automatic stack allocation).  */
	sp -= 20;

	/* This isn't the same signal frame as the kernel uses, because
           we don't want to bother getting all registers on and off the
           stack.  */

	/* First, we store the currently blocked signals.  */
	blocked = 0;
	for (i = 0; i < 32; i++)
	  blocked
	    |= current_cpu->thread_data[next].sigdata[i + 1].blocked << i;
	sim_core_write_aligned_4 (current_cpu, pc, 0, sp, blocked);
	blocked = 0;
	for (i = 0; i < 31; i++)
	  blocked
	    |= current_cpu->thread_data[next].sigdata[i + 33].blocked << i;
	sim_core_write_aligned_4 (current_cpu, pc, 0, sp + 4, blocked);

	/* Then, the actual instructions.  This is CPU-specific, but we
	   use instructions from the common subset for v10 and v32 which
	   should be safe for the time being but could be parametrized
	   if need be.  */
	/* MOVU.W [PC+],R9.  */
	sim_core_write_aligned_2 (current_cpu, pc, 0, sp + 8, 0x9c5f);
	/* .WORD TARGET_SYS_sigreturn.  */
	sim_core_write_aligned_2 (current_cpu, pc, 0, sp + 10,
				  TARGET_SYS_sigreturn);
	/* BREAK 13.  */
	sim_core_write_aligned_2 (current_cpu, pc, 0, sp + 12, 0xe93d);

	/* NOP (on v32; it's SETF on v10, but is the correct compatible
	   instruction.  Still, it doesn't matter because v10 has no
	   delay slot for BREAK so it will not be executed).  */
	sim_core_write_aligned_2 (current_cpu, pc, 0, sp + 16, 0x05b0);

	/* Modify registers to hold the right values for the sighandler
	   context: updated stackpointer and return address pointing to
	   the sigreturn stub.  */
	bfd_putl32 (sp, regbuf);
	(*CPU_REG_STORE (current_cpu)) (current_cpu, H_GR_SP, regbuf, 4);
	bfd_putl32 (sp + 8, regbuf);
	(*CPU_REG_STORE (current_cpu)) (current_cpu, TARGET_SRP_REGNUM,
					regbuf, 4);

	current_cpu->thread_data[next].sigdata[sig].pending = 0;

	/* Block this signal (for the duration of the sighandler).  */
	current_cpu->thread_data[next].sigdata[sig].blocked = 1;

	sim_pc_set (current_cpu, current_cpu->sighandler[sig]);
	bfd_putl32 (sig, regbuf);
	(*CPU_REG_STORE (current_cpu)) (current_cpu, H_GR_R10,
					regbuf, 4);

	/* We ignore a SA_SIGINFO flag in the sigaction call; the code I
	   needed all this for, specifies a SA_SIGINFO call but treats it
	   like an ordinary sighandler; only the signal number argument is
	   inspected.  To make future need to implement SA_SIGINFO
	   correctly possible, we set the siginfo argument register to a
	   magic (hopefully non-address) number.  (NB: then, you should
	   just need to pass the siginfo argument; it seems you probably
	   don't need to implement the specific rt_sigreturn.)  */
	bfd_putl32 (0xbad5161f, regbuf);
	(*CPU_REG_STORE (current_cpu)) (current_cpu, H_GR_R11,
					regbuf, 4);

	/* The third argument is unused and the kernel sets it to 0.  */
	bfd_putl32 (0, regbuf);
	(*CPU_REG_STORE (current_cpu)) (current_cpu, H_GR_R12,
					regbuf, 4);
	return;
      }

    /* No, there actually was no pending signal for this thread.  Reset
       this flag.  */
    current_cpu->thread_data[next].sigpending = 0;
  }
}

/* Reschedule the simplest possible way until something else is absolutely
   necessary:
   - A. Find the next process (round-robin) that doesn't have at_syscall
        set, schedule it.
   - B. If there is none, just run the next process, round-robin.
   - Clear at_syscall for the current process.  */

static void
reschedule (SIM_CPU *current_cpu)
{
  int i;

  /* Iterate over all thread slots, because after a few thread creations
     and exits, we don't know where the live ones are.  */
  for (i = (current_cpu->threadno + 1) % SIM_TARGET_MAX_THREADS;
       i != current_cpu->threadno;
       i = (i + 1) % SIM_TARGET_MAX_THREADS)
    if (current_cpu->thread_data[i].cpu_context
	&& current_cpu->thread_data[i].at_syscall == 0)
      {
	schedule (current_cpu, i);
	return;
      }

  /* Pick any next live thread.  */
  for (i = (current_cpu->threadno + 1) % SIM_TARGET_MAX_THREADS;
       i != current_cpu->threadno;
       i = (i + 1) % SIM_TARGET_MAX_THREADS)
    if (current_cpu->thread_data[i].cpu_context)
      {
	schedule (current_cpu, i);
	return;
      }

  /* More than one live thread, but we couldn't find the next one?  */
  abort ();
}

/* Set up everything to receive (or IGN) an incoming signal to the
   current context.  */

static int
deliver_signal (SIM_CPU *current_cpu, int sig, unsigned int pid)
{
  int i;
  USI pc = sim_pc_get (current_cpu);

  /* Find the thread index of the pid. */
  for (i = 0; i < SIM_TARGET_MAX_THREADS; i++)
    /* Apparently it's ok to send signals to zombies (so a check for
       current_cpu->thread_data[i].cpu_context != NULL would be
       wrong). */
    if (current_cpu->thread_data[i].threadid == pid - TARGET_PID)
      {
	if (sig < 64)
	  switch (current_cpu->sighandler[sig])
	    {
	    case TARGET_SIG_DFL:
	      switch (sig)
		{
		  /* The following according to the glibc
		     documentation. (The kernel code has non-obvious
		     execution paths.)  */
		case TARGET_SIGFPE:
		case TARGET_SIGILL:
		case TARGET_SIGSEGV:
		case TARGET_SIGBUS:
		case TARGET_SIGABRT:
		case TARGET_SIGTRAP:
		case TARGET_SIGSYS:

		case TARGET_SIGTERM:
		case TARGET_SIGINT:
		case TARGET_SIGQUIT:
		case TARGET_SIGKILL:
		case TARGET_SIGHUP:

		case TARGET_SIGALRM:
		case TARGET_SIGVTALRM:
		case TARGET_SIGPROF:
		case TARGET_SIGSTOP:

		case TARGET_SIGPIPE:
		case TARGET_SIGLOST:
		case TARGET_SIGXCPU:
		case TARGET_SIGXFSZ:
		case TARGET_SIGUSR1:
		case TARGET_SIGUSR2:
		  sim_io_eprintf (CPU_STATE (current_cpu),
				  "Exiting pid %d due to signal %d\n",
				  pid, sig);
		  sim_engine_halt (CPU_STATE (current_cpu), current_cpu,
				   NULL, pc, sim_stopped,
				   sig == TARGET_SIGABRT
				   ? SIM_SIGABRT : SIM_SIGILL);
		  return 0;

		  /* The default for all other signals is to be ignored.  */
		default:
		  return 0;
		}

	    case TARGET_SIG_IGN:
	      switch (sig)
		{
		case TARGET_SIGKILL:
		case TARGET_SIGSTOP:
		  /* Can't ignore these signals.  */
		  sim_io_eprintf (CPU_STATE (current_cpu),
				  "Exiting pid %d due to signal %d\n",
				  pid, sig);
		  sim_engine_halt (CPU_STATE (current_cpu), current_cpu,
				   NULL, pc, sim_stopped, SIM_SIGILL);
		  return 0;

		default:
		  return 0;
		}
	      break;

	    default:
	      /* Mark the signal as pending, making schedule () check
		 closer.  The signal will be handled when the thread is
		 scheduled and the signal is unblocked.  */
	      current_cpu->thread_data[i].sigdata[sig].pending = 1;
	      current_cpu->thread_data[i].sigpending = 1;
	      return 0;
	    }
	else
	  {
	    sim_io_eprintf (CPU_STATE (current_cpu),
			    "Unimplemented signal: %d\n", sig);
	    sim_engine_halt (CPU_STATE (current_cpu), current_cpu, NULL, pc,
			     sim_stopped, SIM_SIGILL);
	  }
      }

  return
    -cb_host_to_target_errno (STATE_CALLBACK (CPU_STATE (current_cpu)),
			      ESRCH);
}

/* Make the vector and the first item, the main thread.  */

static void
make_first_thread (SIM_CPU *current_cpu)
{
  current_cpu->thread_data
    = xcalloc (1,
	       SIM_TARGET_MAX_THREADS
	       * sizeof (current_cpu->thread_data[0]));
  current_cpu->thread_data[0].cpu_context
    = (*current_cpu->make_thread_cpu_data) (current_cpu,
					    &current_cpu
					    ->cpu_data_placeholder);
  current_cpu->thread_data[0].parent_threadid = -1;

  /* For good measure.  */
  if (TARGET_SIG_DFL != 0)
    abort ();
}

/* Handle unknown system calls.  Returns (if it does) the syscall
   return value.  */

static USI
cris_unknown_syscall (SIM_CPU *current_cpu, USI pc, char *s, ...)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  host_callback *cb = STATE_CALLBACK (sd);

  if (cris_unknown_syscall_action == CRIS_USYSC_MSG_STOP
      || cris_unknown_syscall_action == CRIS_USYSC_MSG_ENOSYS)
    {
      va_list ap;

      va_start (ap, s);
      sim_io_evprintf (sd, s, ap);
      va_end (ap);

      if (cris_unknown_syscall_action == CRIS_USYSC_MSG_STOP)
	sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGILL);
    }

  return -cb_host_to_target_errno (cb, ENOSYS);
}

/* Main function: the handler of the "break 13" syscall insn.  */

USI
cris_break_13_handler (SIM_CPU *current_cpu, USI callnum, USI arg1,
		       USI arg2, USI arg3, USI arg4, USI arg5, USI arg6,
		       USI pc)
{
  CB_SYSCALL s;
  SIM_DESC sd = CPU_STATE (current_cpu);
  host_callback *cb = STATE_CALLBACK (sd);
  int retval;
  int threadno = current_cpu->threadno;

  current_cpu->syscalls++;

  CB_SYSCALL_INIT (&s);
  s.func = callnum;
  s.arg1 = arg1;
  s.arg2 = arg2;
  s.arg3 = arg3;

  if (callnum == TARGET_SYS_exit && current_cpu->m1threads == 0)
    {
      if (CPU_CRIS_MISC_PROFILE (current_cpu)->flags
	  & FLAG_CRIS_MISC_PROFILE_ALL)
	dump_statistics (current_cpu);
      sim_engine_halt (sd, current_cpu, NULL, pc, sim_exited, arg1);
    }

  s.p1 = (PTR) sd;
  s.p2 = (PTR) current_cpu;
  s.read_mem = syscall_read_mem;
  s.write_mem = syscall_write_mem;

  current_cpu_for_cb_callback = current_cpu;

  if (cb_syscall (cb, &s) != CB_RC_OK)
    {
      abort ();
      sim_io_eprintf (sd, "Break 13: invalid %d?  Returned %ld\n", callnum,
		      s.result);
      sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGILL);
    }

  retval = s.result == -1 ? -s.errcode : s.result;

  if (s.errcode != 0 && s.errcode == cb_host_to_target_errno (cb, ENOSYS))
    {
      /* If the generic simulator call said ENOSYS, then let's try the
	 ones we know ourselves.

	 The convention is to provide *very limited* functionality on an
	 as-needed basis, only what's covered by the test-suite, tests
	 added when functionality changes and abort with a descriptive
	 message for *everything* else.  Where there's no test-case, we
	 just abort.  */
      switch (callnum)
	{
	case 0:
	  /* It's a pretty safe bet that the "old setup() system call"
	     number will not be re-used; we can't say the same for higher
	     numbers.  We treat this simulator-generated call as "wait
	     forever"; we re-run this insn.  The wait is ended by a
	     callback.  Sanity check that this is the reason we got
	     here. */
	  if (current_cpu->thread_data == NULL
	      || (current_cpu->thread_data[threadno].pipe_write_fd == 0))
	    goto unimplemented_syscall;

	  sim_pc_set (current_cpu, pc);
	  retval = arg1;
	  break;

	case TARGET_SYS_fcntl64:
	case TARGET_SYS_fcntl:
	  switch (arg2)
	    {
	    case 1:
	      /* F_GETFD.
		 Glibc checks stdin, stdout and stderr fd:s for
		 close-on-exec security sanity.  We just need to provide a
		 OK return value.  If we really need to have a
		 close-on-exec flag true, we could just do a real fcntl
		 here.  */
	      retval = 0;
	      break;

	    case 2:
	      /* F_SETFD.  Just ignore attempts to set the close-on-exec
		 flag.  */
	      retval = 0;
	      break;

	    case 3:
	      /* F_GETFL.  Check for the special case for open+fdopen.  */
	      if (current_cpu->last_syscall == TARGET_SYS_open
		  && arg1 == current_cpu->last_open_fd)
		{
		  retval = current_cpu->last_open_flags & TARGET_O_ACCMODE;
		  break;
		}
	      else if (arg1 == 0)
		{
		  /* Because we can't freopen fd:s 0, 1, 2 to mean
		     something else than stdin, stdout and stderr
		     (sim/common/syscall.c:cb_syscall special cases fd
		     0, 1 and 2), we know what flags that we can
		     sanely return for these fd:s.  */
		  retval = TARGET_O_RDONLY;
		  break;
		}
	      else if (arg1 == 1 || arg1 == 2)
		{
		  retval = TARGET_O_WRONLY;
		  break;
		}
	      /* FALLTHROUGH */
	    default:
	      /* Nothing else is implemented.  */
	      retval
		= cris_unknown_syscall (current_cpu, pc,
					"Unimplemented %s syscall "
					"(fd: 0x%lx: cmd: 0x%lx arg: "
					"0x%lx)\n",
					callnum == TARGET_SYS_fcntl
					? "fcntl" : "fcntl64",
					(unsigned long) (USI) arg1,
					(unsigned long) (USI) arg2,
					(unsigned long) (USI) arg3);
	      break;
	    }
	  break;

	case TARGET_SYS_uname:
	  {
	    /* Fill in a few constants to appease glibc.  */
	    static const char sim_utsname[6][65] =
	    {
	      "Linux",
	      "sim-target",
	      "2.4.5",
	      TARGET_UTSNAME,
	      "cris",
	      "localdomain"
	    };

	    if ((s.write_mem) (cb, &s, arg1, (const char *) sim_utsname,
			       sizeof (sim_utsname))
		!= sizeof (sim_utsname))
	      retval = -cb_host_to_target_errno (cb, EFAULT);
	    else
	      retval = 0;
	    break;
	  }

	case TARGET_SYS_geteuid32:
	  /* We tell the truth with these.  Maybe we shouldn't, but it
	     should match the "stat" information.  */
	  retval = geteuid ();
	  break;

	case TARGET_SYS_getuid32:
	  retval = getuid ();
	  break;

	case TARGET_SYS_getegid32:
	  retval = getegid ();
	  break;

	case TARGET_SYS_getgid32:
	  retval = getgid ();
	  break;

	case TARGET_SYS_brk:
	  /* Most often, we just return the argument, like the Linux
	     kernel.  */
	  retval = arg1;

	  if (arg1 == 0)
	    retval = current_cpu->endbrk;
	  else if (arg1 <= current_cpu->endmem)
	    current_cpu->endbrk = arg1;
	  else
	    {
	      USI new_end = (arg1 + 8191) & ~8191;

	      /* If the simulator wants to brk more than a certain very
		 large amount, something is wrong.  FIXME: Return an error
		 or abort?  Have command-line selectable?  */
	      if (new_end - current_cpu->endmem > SIM_MAX_ALLOC_CHUNK)
		{
		  current_cpu->endbrk = current_cpu->endmem;
		  retval = current_cpu->endmem;
		  break;
		}

	      sim_core_attach (sd, NULL, 0, access_read_write_exec, 0,
			       current_cpu->endmem,
			       new_end - current_cpu->endmem,
			       0, NULL, NULL);
	      current_cpu->endbrk = arg1;
	      current_cpu->endmem = new_end;
	    }
	  break;

	case TARGET_SYS_getpid:
	  /* Correct until CLONE_THREAD is implemented.  */
	  retval = current_cpu->thread_data == NULL
	    ? TARGET_PID
	    : TARGET_PID + current_cpu->thread_data[threadno].threadid;
	  break;

	case TARGET_SYS_getppid:
	  /* Correct until CLONE_THREAD is implemented.  */
	  retval = current_cpu->thread_data == NULL
	    ? TARGET_PID - 1
	    : (TARGET_PID
	       + current_cpu->thread_data[threadno].parent_threadid);
	  break;

	case TARGET_SYS_mmap2:
	  {
	    USI addr = arg1;
	    USI len = arg2;
	    USI prot = arg3;
	    USI flags = arg4;
	    USI fd = arg5;
	    USI pgoff = arg6;

	    /* If the simulator wants to mmap more than the very large
	       limit, something is wrong.  FIXME: Return an error or
	       abort?  Have command-line selectable?  */
	    if (len > SIM_MAX_ALLOC_CHUNK)
	      {
		retval = -cb_host_to_target_errno (cb, ENOMEM);
		break;
	      }

	    if ((prot != (TARGET_PROT_READ | TARGET_PROT_WRITE)
		 && (prot
		     != (TARGET_PROT_READ
			 | TARGET_PROT_WRITE
			 | TARGET_PROT_EXEC))
		 && prot != TARGET_PROT_READ)
		|| (flags != (TARGET_MAP_ANONYMOUS | TARGET_MAP_PRIVATE)
		    && flags != TARGET_MAP_PRIVATE
		    && flags != TARGET_MAP_SHARED)
		|| (fd != (USI) -1 && prot != TARGET_PROT_READ)
		|| pgoff != 0)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
						 "Unimplemented mmap2 call "
						 "(0x%lx, 0x%lx, 0x%lx, "
						 "0x%lx, 0x%lx, 0x%lx)\n",
						 (unsigned long) arg1,
						 (unsigned long) arg2,
						 (unsigned long) arg3,
						 (unsigned long) arg4,
						 (unsigned long) arg5,
						 (unsigned long) arg6);
		break;
	      }
	    else if (fd != (USI) -1)
	      {
		/* Map a file.  */

		USI newaddr;
		USI pos;

		/* A non-aligned argument is allowed for files.  */
		USI newlen = (len + 8191) & ~8191;

		/* We only support read, which we should already have
		   checked.  Check again anyway.  */
		if (prot != TARGET_PROT_READ)
		  abort ();

		newaddr
		  = create_map (sd, &current_cpu->highest_mmapped_page, addr,
				newlen);

		if (newaddr >= (USI) -8191)
		  {
		    abort ();
		    retval = -cb_host_to_target_errno (cb, -(SI) newaddr);
		    break;
		  }

		/* Find the current position in the file.  */
		s.func = TARGET_SYS_lseek;
		s.arg1 = fd;
		s.arg2 = 0;
		s.arg3 = SEEK_CUR;
		if (cb_syscall (cb, &s) != CB_RC_OK)
		  abort ();
		pos = s.result;

		if (s.result < 0)
		  abort ();

		/* Use the standard read callback to read in "len"
		   bytes.  */
		s.func = TARGET_SYS_read;
		s.arg1 = fd;
		s.arg2 = newaddr;
		s.arg3 = len;
		if (cb_syscall (cb, &s) != CB_RC_OK)
		  abort ();

		if ((USI) s.result != len)
		  abort ();

		/* After reading, we need to go back to the previous
                   position in the file.  */
		s.func = TARGET_SYS_lseek;
		s.arg1 = fd;
		s.arg2 = pos;
		s.arg3 = SEEK_SET;
		if (cb_syscall (cb, &s) != CB_RC_OK)
		  abort ();
		if (pos != (USI) s.result)
		  abort ();

		retval = newaddr;
	      }
	    else
	      {
		USI newaddr
		  = create_map (sd, &current_cpu->highest_mmapped_page, addr,
				(len + 8191) & ~8191);

		if (newaddr >= (USI) -8191)
		  retval = -cb_host_to_target_errno (cb, -(SI) newaddr);
		else
		  retval = newaddr;
	      }
	    break;
	  }

	case TARGET_SYS_mprotect:
	  {
	    /* We only cover the case of linuxthreads mprotecting out its
	       stack guard page.  */
	    USI addr = arg1;
	    USI len = arg2;
	    USI prot = arg3;

	    if ((addr & 8191) != 0
		|| len != 8192
		|| prot != TARGET_PROT_NONE
		|| !is_mapped (sd, &current_cpu->highest_mmapped_page, addr,
			       len))
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented mprotect call "
					  "(0x%lx, 0x%lx, 0x%lx)\n",
					  (unsigned long) arg1,
					  (unsigned long) arg2,
					  (unsigned long) arg3);
		break;
	      }

	    /* FIXME: We should account for pages like this that are
	       "mprotected out".  For now, we just tell the simulator
	       core to remove that page from its map.  */
	    sim_core_detach (sd, NULL, 0, 0, addr);
	    retval = 0;
	    break;
	  }

	case TARGET_SYS_ioctl:
	  {
	    /* We support only a very limited functionality: checking
	       stdout with TCGETS to perform the isatty function.  The
	       TCGETS ioctl isn't actually performed or the result used by
	       an isatty () caller in a "hello, world" program; only the
	       return value is then used.  Maybe we shouldn't care about
	       the environment of the simulator regarding isatty, but
	       that's been working before, in the xsim simulator.  */
	    if (arg2 == TARGET_TCGETS && arg1 == 1)
	      retval = isatty (1) ? 0 : cb_host_to_target_errno (cb, EINVAL);
	    else
	      retval = -cb_host_to_target_errno (cb, EINVAL);
	    break;
	  }

	case TARGET_SYS_munmap:
	  {
	    USI addr = arg1;
	    USI len = arg2;
	    USI result
	      = unmap_pages (sd, &current_cpu->highest_mmapped_page, addr,
			     len);
	    retval = result != 0 ? -cb_host_to_target_errno (cb, result) : 0;
	    break;
	  }

	case TARGET_SYS_wait4:
	  {
	    int i;
	    USI pid = arg1;
	    USI saddr = arg2;
	    USI options = arg3;
	    USI rusagep = arg4;

	    /* FIXME: We're not properly implementing __WCLONE, and we
	       don't really need the special casing so we might as well
	       make this general.  */
	    if ((!(pid == (USI) -1
		   && options == (TARGET___WCLONE | TARGET_WNOHANG)
		   && saddr != 0)
		 && !(pid > 0
		      && (options == TARGET___WCLONE
			  || options == TARGET___WALL)))
		|| rusagep != 0
		|| current_cpu->thread_data == NULL)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented wait4 call "
					  "(0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
					  (unsigned long) arg1,
					  (unsigned long) arg2,
					  (unsigned long) arg3,
					  (unsigned long) arg4);
		break;
	      }

	    if (pid == (USI) -1)
	      for (i = 1; i < SIM_TARGET_MAX_THREADS; i++)
		{
		  if (current_cpu->thread_data[threadno].threadid
		      == current_cpu->thread_data[i].parent_threadid
		      && current_cpu->thread_data[i].threadid != 0
		      && current_cpu->thread_data[i].cpu_context == NULL)
		    {
		      /* A zombied child.  Get the exit value and clear the
			 zombied entry so it will be reused.  */
		      sim_core_write_unaligned_4 (current_cpu, pc, 0, saddr,
						  current_cpu
						  ->thread_data[i].exitval);
		      retval
			= current_cpu->thread_data[i].threadid + TARGET_PID;
		      memset (&current_cpu->thread_data[i], 0,
			      sizeof (current_cpu->thread_data[i]));
		      goto outer_break;
		    }
		}
	    else
	      {
		/* We're waiting for a specific PID.  If we don't find
		   it zombied on this run, rerun the syscall.  */
		for (i = 1; i < SIM_TARGET_MAX_THREADS; i++)
		  if (pid == current_cpu->thread_data[i].threadid + TARGET_PID
		      && current_cpu->thread_data[i].cpu_context == NULL)
		    {
		      if (saddr != 0)
			/* Get the exit value if the caller wants it.  */
			sim_core_write_unaligned_4 (current_cpu, pc, 0,
						    saddr,
						    current_cpu
						    ->thread_data[i]
						    .exitval);

		      retval
			= current_cpu->thread_data[i].threadid + TARGET_PID;
		      memset (&current_cpu->thread_data[i], 0,
			      sizeof (current_cpu->thread_data[i]));

		      goto outer_break;
		    }

		sim_pc_set (current_cpu, pc);
	      }

	    retval = -cb_host_to_target_errno (cb, ECHILD);
	  outer_break:
	    break;
	  }

	case TARGET_SYS_rt_sigaction:
	  {
	    USI signum = arg1;
	    USI old_sa = arg3;
	    USI new_sa = arg2;

	    /* The kernel says:
	       struct sigaction {
			__sighandler_t sa_handler;
			unsigned long sa_flags;
			void (*sa_restorer)(void);
			sigset_t sa_mask;
	       }; */

	    if (old_sa != 0)
	      {
		sim_core_write_unaligned_4 (current_cpu, pc, 0, old_sa + 0,
					    current_cpu->sighandler[signum]);
		sim_core_write_unaligned_4 (current_cpu, pc, 0, arg3 + 4, 0);
		sim_core_write_unaligned_4 (current_cpu, pc, 0, arg3 + 8, 0);

		/* We'll assume _NSIG_WORDS is 2 for the kernel.  */
		sim_core_write_unaligned_4 (current_cpu, pc, 0, arg3 + 12, 0);
		sim_core_write_unaligned_4 (current_cpu, pc, 0, arg3 + 16, 0);
	      }
	    if (new_sa != 0)
	      {
		USI target_sa_handler
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, new_sa);
		USI target_sa_flags
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, new_sa + 4);
		USI target_sa_restorer
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, new_sa + 8);
		USI target_sa_mask_low
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, new_sa + 12);
		USI target_sa_mask_high
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, new_sa + 16);

		/* We won't interrupt a syscall so we won't restart it,
		   but a signal(2) call ends up syscalling rt_sigaction
		   with this flag, so we have to handle it.  The
		   sa_restorer field contains garbage when not
		   TARGET_SA_RESTORER, so don't look at it.  For the
		   time being, we don't nest sighandlers, so we
		   ignore the sa_mask, which simplifies things.  */
		if ((target_sa_flags != 0
		     && target_sa_flags != TARGET_SA_RESTART
		     && target_sa_flags != (TARGET_SA_RESTART|TARGET_SA_SIGINFO))
		    || target_sa_handler == 0)
		  {
		    retval
		      = cris_unknown_syscall (current_cpu, pc,
					      "Unimplemented rt_sigaction "
					      "syscall "
					      "(0x%lx, 0x%lx: "
					      "[0x%x, 0x%x, 0x%x, "
					      "{0x%x, 0x%x}], 0x%lx)\n",
					      (unsigned long) arg1,
					      (unsigned long) arg2,
					      target_sa_handler,
					      target_sa_flags,
					      target_sa_restorer,
					      target_sa_mask_low,
					      target_sa_mask_high,
					      (unsigned long) arg3);
		    break;
		  }

		current_cpu->sighandler[signum] = target_sa_handler;

		/* Because we may have unblocked signals, one may now be
		   pending, if there are threads, that is.  */
		if (current_cpu->thread_data)
		  current_cpu->thread_data[threadno].sigpending = 1;
	      }
	    retval = 0;
	    break;
	  }

	case TARGET_SYS_mremap:
	  {
	    USI addr = arg1;
	    USI old_len = arg2;
	    USI new_len = arg3;
	    USI flags = arg4;
	    USI new_addr = arg5;
	    USI mapped_addr;

	    if (new_len == old_len)
	      /* The program and/or library is possibly confused but
		 this is a valid call.  Happens with ipps-1.40 on file
		 svs_all.  */
	      retval = addr;
	    else if (new_len < old_len)
	      {
		/* Shrinking is easy.  */
		if (unmap_pages (sd, &current_cpu->highest_mmapped_page,
				 addr + new_len, old_len - new_len) != 0)
		  retval = -cb_host_to_target_errno (cb, EINVAL);
		else
		  retval = addr;
	      }
	    else if (! is_mapped (sd, &current_cpu->highest_mmapped_page,
				  addr + old_len, new_len - old_len))
	      {
		/* If the extension isn't mapped, we can just add it.  */
		mapped_addr
		  = create_map (sd, &current_cpu->highest_mmapped_page,
				addr + old_len, new_len - old_len);

		if (mapped_addr > (USI) -8192)
		  retval = -cb_host_to_target_errno (cb, -(SI) mapped_addr);
		else
		  retval = addr;
	      }
	    else if (flags & TARGET_MREMAP_MAYMOVE)
	      {
		/* Create a whole new map and copy the contents
		   block-by-block there.  We ignore the new_addr argument
		   for now.  */
		char buf[8192];
		USI prev_addr = addr;
		USI prev_len = old_len;

		mapped_addr
		  = create_map (sd, &current_cpu->highest_mmapped_page,
				0, new_len);

		if (mapped_addr > (USI) -8192)
		  {
		    retval = -cb_host_to_target_errno (cb, -(SI) new_addr);
		    break;
		  }

		retval = mapped_addr;

		for (; old_len > 0;
		     old_len -= 8192, mapped_addr += 8192, addr += 8192)
		  {
		    if (sim_core_read_buffer (sd, current_cpu, read_map, buf,
					      addr, 8192) != 8192
			|| sim_core_write_buffer (sd, current_cpu, 0, buf,
						  mapped_addr, 8192) != 8192)
		      abort ();
		  }

		if (unmap_pages (sd, &current_cpu->highest_mmapped_page,
				 prev_addr, prev_len) != 0)
		  abort ();
	      }
	    else
	      retval = -cb_host_to_target_errno (cb, -ENOMEM);
	    break;
	  }

	case TARGET_SYS_poll:
	  {
	    int npollfds = arg2;
	    int timeout = arg3;
	    SI ufds = arg1;
	    SI fd = -1;
	    HI events = -1;
	    HI revents = 0;
	    struct stat buf;
	    int i;

	    /* The kernel says:
		struct pollfd {
		     int fd;
		     short events;
		     short revents;
		}; */

	    /* Check that this is the expected poll call from
	       linuxthreads/manager.c; we don't support anything else.
	       Remember, fd == 0 isn't supported.  */
	    if (npollfds != 1
		|| ((fd = sim_core_read_unaligned_4 (current_cpu, pc,
						     0, ufds)) <= 0)
		|| ((events = sim_core_read_unaligned_2 (current_cpu, pc,
							 0, ufds + 4))
		    != TARGET_POLLIN)
		|| ((cb->fstat) (cb, fd, &buf) != 0
		    || (buf.st_mode & S_IFIFO) == 0)
		|| current_cpu->thread_data == NULL)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented poll syscall "
					  "(0x%lx: [0x%x, 0x%x, x], "
					  "0x%lx, 0x%lx)\n",
					  (unsigned long) arg1, fd, events,
					  (unsigned long) arg2,
					  (unsigned long) arg3);
		break;
	      }

	    retval = 0;

	    /* Iterate over threads; find a marker that a writer is
	       sleeping, waiting for a reader.  */
	    for (i = 0; i < SIM_TARGET_MAX_THREADS; i++)
	      if (current_cpu->thread_data[i].cpu_context != NULL
		  && current_cpu->thread_data[i].pipe_read_fd == fd)
		{
		  revents = TARGET_POLLIN;
		  retval = 1;
		  break;
		}

	    /* Timeout decreases with whatever time passed between the
	       last syscall and this.  That's not exactly right for the
	       first call, but it's close enough that it isn't
	       worthwhile to complicate matters by making that a special
	       case.  */
	    timeout
	      -= (TARGET_TIME_MS (current_cpu)
		  - (current_cpu->thread_data[threadno].last_execution));

	    /* Arrange to repeat this syscall until timeout or event,
               decreasing timeout at each iteration.  */
	    if (timeout > 0 && revents == 0)
	      {
		bfd_byte timeout_buf[4];

		bfd_putl32 (timeout, timeout_buf);
		(*CPU_REG_STORE (current_cpu)) (current_cpu,
						H_GR_R12, timeout_buf, 4);
		sim_pc_set (current_cpu, pc);
		retval = arg1;
		break;
	      }

	    sim_core_write_unaligned_2 (current_cpu, pc, 0, ufds + 4 + 2,
					revents);
	    break;
	  }

	case TARGET_SYS_time:
	  {
	    retval = (int) (*cb->time) (cb, 0L);

	    /* At time of this writing, CB_SYSCALL_time doesn't do the
	       part of setting *arg1 to the return value.  */
	    if (arg1)
	      sim_core_write_unaligned_4 (current_cpu, pc, 0, arg1, retval);
	    break;
	  }

	case TARGET_SYS_gettimeofday:
	  if (arg1 != 0)
	    {
	      USI ts = TARGET_TIME (current_cpu);
	      USI tms = TARGET_TIME_MS (current_cpu);

	      /* First dword is seconds since TARGET_EPOCH.  */
	      sim_core_write_unaligned_4 (current_cpu, pc, 0, arg1, ts);

	      /* Second dword is microseconds.  */
	      sim_core_write_unaligned_4 (current_cpu, pc, 0, arg1 + 4,
					  (tms % 1000) * 1000);
	    }
	  if (arg2 != 0)
	    {
	      /* Time-zone info is always cleared.  */
	      sim_core_write_unaligned_4 (current_cpu, pc, 0, arg2, 0);
	      sim_core_write_unaligned_4 (current_cpu, pc, 0, arg2 + 4, 0);
	    }
	  retval = 0;
	  break;

	case TARGET_SYS_llseek:
	  {
	    /* If it fits, tweak parameters to fit the "generic" 32-bit
	       lseek and use that.  */
	    SI fd = arg1;
	    SI offs_hi = arg2;
	    SI offs_lo = arg3;
	    SI resultp = arg4;
	    SI whence = arg5;
	    retval = 0;

	    if (!((offs_hi == 0 && offs_lo >= 0)
		  || (offs_hi == -1 &&  offs_lo < 0)))
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented llseek offset,"
					  " fd %d: 0x%x:0x%x\n",
					  fd, (unsigned) arg2,
					  (unsigned) arg3);
		break;
	      }

	    s.func = TARGET_SYS_lseek;
	    s.arg2 = offs_lo;
	    s.arg3 = whence;
	    if (cb_syscall (cb, &s) != CB_RC_OK)
	      {
		sim_io_eprintf (sd, "Break 13: invalid %d?  Returned %ld\n", callnum,
				s.result);
		sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGILL);
	      }
	    if (s.result < 0)
	      retval = -s.errcode;
	    else
	      {
		sim_core_write_unaligned_4 (current_cpu, pc, 0, resultp,
					    s.result);
		sim_core_write_unaligned_4 (current_cpu, pc, 0, resultp + 4,
					    s.result < 0 ? -1 : 0);
	      }
	    break;
	  }

	/* This one does have a generic callback function, but at the time
	   of this writing, cb_syscall does not have code for it, and we
	   need target-specific code for the threads implementation
	   anyway.  */
	case TARGET_SYS_kill:
	  {
	    USI pid = arg1;
	    USI sig = arg2;

	    retval = 0;

	    /* At kill(2), glibc sets signal masks such that the thread
	       machinery is initialized.  Still, there is and was only
	       one thread.  */
	    if (current_cpu->max_threadid == 0)
	      {
		if (pid != TARGET_PID)
		  {
		    retval = -cb_host_to_target_errno (cb, EPERM);
		    break;
		  }

		/* FIXME: Signal infrastructure (target-to-sim mapping).  */
		if (sig == TARGET_SIGABRT)
		  /* A call "abort ()", i.e. "kill (getpid(), SIGABRT)" is
		     the end-point for failing GCC test-cases.  */
		  sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped,
				   SIM_SIGABRT);
		else
		  {
		    sim_io_eprintf (sd, "Unimplemented signal: %d\n", sig);
		    sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped,
				     SIM_SIGILL);
		  }

		/* This will not be reached.  */
		abort ();
	      }
	    else
	      retval = deliver_signal (current_cpu, sig, pid);
	    break;
	  }

	case TARGET_SYS_rt_sigprocmask:
	  {
	    int i;
	    USI how = arg1;
	    USI newsetp = arg2;
	    USI oldsetp = arg3;

	    if (how != TARGET_SIG_BLOCK
		&& how != TARGET_SIG_SETMASK
		&& how != TARGET_SIG_UNBLOCK)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented rt_sigprocmask "
					  "syscall (0x%x, 0x%x, 0x%x)\n",
					  arg1, arg2, arg3);
		break;
	      }

	    if (newsetp)
	      {
		USI set_low
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0,
					       newsetp);
		USI set_high
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0,
					       newsetp + 4);

		/* The sigmask is kept in the per-thread data, so we may
		   need to create the first one.  */
		if (current_cpu->thread_data == NULL)
		  make_first_thread (current_cpu);

		if (how == TARGET_SIG_SETMASK)
		  for (i = 0; i < 64; i++)
		    current_cpu->thread_data[threadno].sigdata[i].blocked = 0;

		for (i = 0; i < 32; i++)
		  if ((set_low & (1 << i)))
		    current_cpu->thread_data[threadno].sigdata[i + 1].blocked
		      = (how != TARGET_SIG_UNBLOCK);

		for (i = 0; i < 31; i++)
		  if ((set_high & (1 << i)))
		    current_cpu->thread_data[threadno].sigdata[i + 33].blocked
		      = (how != TARGET_SIG_UNBLOCK);

		/* The mask changed, so a signal may be unblocked for
                   execution.  */
		current_cpu->thread_data[threadno].sigpending = 1;
	      }

	    if (oldsetp != 0)
	      {
		USI set_low = 0;
		USI set_high = 0;

		for (i = 0; i < 32; i++)
		  if (current_cpu->thread_data[threadno]
		      .sigdata[i + 1].blocked)
		    set_low |= 1 << i;
		for (i = 0; i < 31; i++)
		  if (current_cpu->thread_data[threadno]
		      .sigdata[i + 33].blocked)
		    set_high |= 1 << i;

		sim_core_write_unaligned_4 (current_cpu, pc, 0, oldsetp + 0, set_low);
		sim_core_write_unaligned_4 (current_cpu, pc, 0, oldsetp + 4, set_high);
	      }

	    retval = 0;
	    break;
	  }

	case TARGET_SYS_sigreturn:
	  {
	    int i;
	    bfd_byte regbuf[4];
	    int was_sigsuspended;

	    if (current_cpu->thread_data == NULL
		/* The CPU context is saved with the simulator data, not
		   on the stack as in the real world.  */
		|| (current_cpu->thread_data[threadno].cpu_context_atsignal
		    == NULL))
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Invalid sigreturn syscall: "
					  "no signal handler active "
					  "(0x%lx, 0x%lx, 0x%lx, 0x%lx, "
					  "0x%lx, 0x%lx)\n",
					  (unsigned long) arg1,
					  (unsigned long) arg2,
					  (unsigned long) arg3,
					  (unsigned long) arg4,
					  (unsigned long) arg5,
					  (unsigned long) arg6);
		break;
	      }

	    was_sigsuspended
	      = current_cpu->thread_data[threadno].sigsuspended;

	    /* Restore the sigmask, either from the stack copy made when
	       the sighandler was called, or from the saved state
	       specifically for sigsuspend(2).  */
	    if (was_sigsuspended)
	      {
		current_cpu->thread_data[threadno].sigsuspended = 0;
		for (i = 0; i < 64; i++)
		  current_cpu->thread_data[threadno].sigdata[i].blocked
		    = current_cpu->thread_data[threadno]
		    .sigdata[i].blocked_suspendsave;
	      }
	    else
	      {
		USI sp;
		USI set_low;
		USI set_high;

		(*CPU_REG_FETCH (current_cpu)) (current_cpu,
					    H_GR_SP, regbuf, 4);
		sp = bfd_getl32 (regbuf);
		set_low
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, sp);
		set_high
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0, sp + 4);

		for (i = 0; i < 32; i++)
		  current_cpu->thread_data[threadno].sigdata[i + 1].blocked
		    = (set_low & (1 << i)) != 0;
		for (i = 0; i < 31; i++)
		  current_cpu->thread_data[threadno].sigdata[i + 33].blocked
		    = (set_high & (1 << i)) != 0;
	      }

	    /* The mask changed, so a signal may be unblocked for
	       execution.  */
	    current_cpu->thread_data[threadno].sigpending = 1;

	    memcpy (&current_cpu->cpu_data_placeholder,
		    current_cpu->thread_data[threadno].cpu_context_atsignal,
		    current_cpu->thread_cpu_data_size);
	    free (current_cpu->thread_data[threadno].cpu_context_atsignal);
	    current_cpu->thread_data[threadno].cpu_context_atsignal = NULL;

	    /* The return value must come from the saved R10.  */
	    (*CPU_REG_FETCH (current_cpu)) (current_cpu, H_GR_R10, regbuf, 4);
	    retval = bfd_getl32 (regbuf);

	    /* We must also break the "sigsuspension loop".  */
	    if (was_sigsuspended)
	      sim_pc_set (current_cpu, sim_pc_get (current_cpu) + 2);
	    break;
	  }

	case TARGET_SYS_rt_sigsuspend:
	  {
	    USI newsetp = arg1;
	    USI setsize = arg2;

	    if (setsize != 8)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented rt_sigsuspend syscall"
					  " arguments (0x%lx, 0x%lx)\n",
					  (unsigned long) arg1,
					  (unsigned long) arg2);
		break;
	      }

	    /* Don't change the signal mask if we're already in
	       sigsuspend state (i.e. this syscall is a rerun).  */
	    else if (!current_cpu->thread_data[threadno].sigsuspended)
	      {
		USI set_low
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0,
					       newsetp);
		USI set_high
		  = sim_core_read_unaligned_4 (current_cpu, pc, 0,
					       newsetp + 4);
		int i;

		/* Save the current sigmask and insert the user-supplied
                   one.  */
		for (i = 0; i < 32; i++)
		  {
		    current_cpu->thread_data[threadno]
		      .sigdata[i + 1].blocked_suspendsave
		      = current_cpu->thread_data[threadno]
		      .sigdata[i + 1].blocked;

		    current_cpu->thread_data[threadno]
		      .sigdata[i + 1].blocked = (set_low & (1 << i)) != 0;
		  }
		for (i = 0; i < 31; i++)
		  {
		    current_cpu->thread_data[threadno]
		      .sigdata[i + 33].blocked_suspendsave
		      = current_cpu->thread_data[threadno]
		      .sigdata[i + 33].blocked;
		    current_cpu->thread_data[threadno]
		      .sigdata[i + 33].blocked = (set_high & (1 << i)) != 0;
		  }

		current_cpu->thread_data[threadno].sigsuspended = 1;

		/* The mask changed, so a signal may be unblocked for
                   execution. */
		current_cpu->thread_data[threadno].sigpending = 1;
	      }

	    /* Because we don't use arg1 (newsetp) when this syscall is
	       rerun, it doesn't matter that we overwrite it with the
	       (constant) return value.  */
	    retval = -cb_host_to_target_errno (cb, EINTR);
	    sim_pc_set (current_cpu, pc);
	    break;
	  }

	  /* Add case labels here for other syscalls using the 32-bit
	     "struct stat", provided they have a corresponding simulator
	     function of course.  */
	case TARGET_SYS_stat:
	case TARGET_SYS_fstat:
	  {
	    /* As long as the infrastructure doesn't cache anything
	       related to the stat mapping, this trick gets us a dual
	       "struct stat"-type mapping in the least error-prone way.  */
	    const char *saved_map = cb->stat_map;
	    CB_TARGET_DEFS_MAP *saved_syscall_map = cb->syscall_map;

	    cb->syscall_map = (CB_TARGET_DEFS_MAP *) syscall_stat32_map;
	    cb->stat_map = stat32_map;

	    if (cb_syscall (cb, &s) != CB_RC_OK)
	      {
		abort ();
		sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped,
				 SIM_SIGILL);
	      }
	    retval = s.result == -1 ? -s.errcode : s.result;

	    cb->stat_map = saved_map;
	    cb->syscall_map = saved_syscall_map;
	    break;
	  }

	case TARGET_SYS_getcwd:
	  {
	    USI buf = arg1;
	    USI size = arg2;

	    char *cwd = xmalloc (SIM_PATHMAX);
	    if (cwd != getcwd (cwd, SIM_PATHMAX))
	      abort ();

	    /* FIXME: When and if we support chdir, we need something
               a bit more elaborate.  */
	    if (simulator_sysroot[0] != '\0')
	      strcpy (cwd, "/");

	    retval = -cb_host_to_target_errno (cb, ERANGE);
	    if (strlen (cwd) + 1 <= size)
	      {
		retval = strlen (cwd) + 1;
		if (sim_core_write_buffer (sd, current_cpu, 0, cwd,
					   buf, retval)
		    != (unsigned int) retval)
		  retval = -cb_host_to_target_errno (cb, EFAULT);
	      }
	    free (cwd);
	    break;
	  }

	case TARGET_SYS_readlink:
	  {
	    SI path = arg1;
	    SI buf = arg2;
	    SI bufsiz = arg3;
	    char *pbuf = xmalloc (SIM_PATHMAX);
	    char *lbuf = xmalloc (SIM_PATHMAX);
	    char *lbuf_alloc = lbuf;
	    int nchars = -1;
	    int i;
	    int o = 0;

	    if (sim_core_read_unaligned_1 (current_cpu, pc, 0, path) == '/')
	      {
		strcpy (pbuf, simulator_sysroot);
		o += strlen (simulator_sysroot);
	      }

	    for (i = 0; i + o < SIM_PATHMAX; i++)
	      {
		pbuf[i + o]
		  = sim_core_read_unaligned_1 (current_cpu, pc, 0, path + i);
		if (pbuf[i + o] == 0)
		  break;
	      }

	    if (i + o == SIM_PATHMAX)
	      {
		retval = -cb_host_to_target_errno (cb, ENAMETOOLONG);
		break;
	      }

	    /* Intervene calls for certain files expected in the target
               proc file system.  */
	    if (strcmp (pbuf + strlen (simulator_sysroot),
			"/proc/" XSTRING (TARGET_PID) "/exe") == 0)
	      {
		char *argv0
		  = (STATE_PROG_ARGV (sd) != NULL
		     ? *STATE_PROG_ARGV (sd) : NULL);

		if (argv0 == NULL || *argv0 == '.')
		  {
		    retval
		      = cris_unknown_syscall (current_cpu, pc,
					      "Unimplemented readlink syscall "
					      "(0x%lx: [\"%s\"], 0x%lx)\n",
					      (unsigned long) arg1, pbuf,
					      (unsigned long) arg2);
		    break;
		  }
		else if (*argv0 == '/')
		  {
		    if (strncmp (simulator_sysroot, argv0,
				 strlen (simulator_sysroot)) == 0)
		      argv0 += strlen (simulator_sysroot);

		    strcpy (lbuf, argv0);
		    nchars = strlen (argv0) + 1;
		  }
		else
		  {
		    if (getcwd (lbuf, SIM_PATHMAX) != NULL
			&& strlen (lbuf) + 2 + strlen (argv0) < SIM_PATHMAX)
		      {
			if (strncmp (simulator_sysroot, lbuf,
				     strlen (simulator_sysroot)) == 0)
			  lbuf += strlen (simulator_sysroot);

			strcat (lbuf, "/");
			strcat (lbuf, argv0);
			nchars = strlen (lbuf) + 1;
		      }
		    else
		      abort ();
		  }
	      }
	    else
	      nchars = readlink (pbuf, lbuf, SIM_PATHMAX);

	    /* We trust that the readlink result returns a *relative*
	       link, or one already adjusted for the file-path-prefix.
	       (We can't generally tell the difference, so we go with
	       the easiest decision; no adjustment.)  */

	    if (nchars == -1)
	      {
		retval = -cb_host_to_target_errno (cb, errno);
		break;
	      }

	    if (bufsiz < nchars)
	      nchars = bufsiz;

	    if (sim_core_write_buffer (sd, current_cpu, write_map, lbuf,
				       buf, nchars) != (unsigned int) nchars)
	      retval = -cb_host_to_target_errno (cb, EFAULT);
	    else
	      retval = nchars;

	    free (pbuf);
	    free (lbuf_alloc);
	    break;
	  }

	case TARGET_SYS_sched_getscheduler:
	  {
	    USI pid = arg1;

	    /* FIXME: Search (other) existing threads.  */
	    if (pid != 0 && pid != TARGET_PID)
	      retval = -cb_host_to_target_errno (cb, ESRCH);
	    else
	      retval = TARGET_SCHED_OTHER;
	    break;
	  }

	case TARGET_SYS_sched_getparam:
	  {
	    USI pid = arg1;
	    USI paramp = arg2;

	    /* The kernel says:
	       struct sched_param {
			int sched_priority;
	       }; */

	    if (pid != 0 && pid != TARGET_PID)
	      retval = -cb_host_to_target_errno (cb, ESRCH);
	    else
	      {
		/* FIXME: Save scheduler setting before threads are
		   created too.  */
		sim_core_write_unaligned_4 (current_cpu, pc, 0, paramp,
					    current_cpu->thread_data != NULL
					    ? (current_cpu
					       ->thread_data[threadno]
					       .priority)
					    : 0);
		retval = 0;
	      }
	    break;
	  }

	case TARGET_SYS_sched_setparam:
	  {
	    USI pid = arg1;
	    USI paramp = arg2;

	    if ((pid != 0 && pid != TARGET_PID)
		|| sim_core_read_unaligned_4 (current_cpu, pc, 0,
					      paramp) != 0)
	      retval = -cb_host_to_target_errno (cb, EINVAL);
	    else
	      retval = 0;
	    break;
	  }

	case TARGET_SYS_sched_setscheduler:
	  {
	    USI pid = arg1;
	    USI policy = arg2;
	    USI paramp = arg3;

	    if ((pid != 0 && pid != TARGET_PID)
		|| policy != TARGET_SCHED_OTHER
		|| sim_core_read_unaligned_4 (current_cpu, pc, 0,
					      paramp) != 0)
	      retval = -cb_host_to_target_errno (cb, EINVAL);
	    else
	      /* FIXME: Save scheduler setting to be read in later
		 sched_getparam calls.  */
	      retval = 0;
	    break;
	  }

	case TARGET_SYS_sched_yield:
	  /* We reschedule to the next thread after a syscall anyway, so
	     we don't have to do anything here than to set the return
	     value.  */
	  retval = 0;
	  break;

	case TARGET_SYS_sched_get_priority_min:
	case TARGET_SYS_sched_get_priority_max:
	  if (arg1 != 0)
	    retval = -cb_host_to_target_errno (cb, EINVAL);
	  else
	    retval = 0;
	  break;

	case TARGET_SYS_ugetrlimit:
	  {
	    unsigned int curlim, maxlim;
	    if (arg1 != TARGET_RLIMIT_STACK && arg1 != TARGET_RLIMIT_NOFILE)
	      {
		retval = -cb_host_to_target_errno (cb, EINVAL);
		break;
	      }

	    /* The kernel says:
	       struct rlimit {
		       unsigned long   rlim_cur;
		       unsigned long   rlim_max;
	       }; */
	    if (arg1 == TARGET_RLIMIT_NOFILE)
	      {
		/* Sadly a very low limit.  Better not lie, though.  */
		maxlim = curlim = MAX_CALLBACK_FDS;
	      }
	    else /* arg1 == TARGET_RLIMIT_STACK */
	      {
		maxlim = 0xffffffff;
		curlim = 0x800000;
	      }
	    sim_core_write_unaligned_4 (current_cpu, pc, 0, arg2, curlim);
	    sim_core_write_unaligned_4 (current_cpu, pc, 0, arg2 + 4, maxlim);
	    retval = 0;
	    break;
	  }

	case TARGET_SYS_setrlimit:
	  if (arg1 != TARGET_RLIMIT_STACK)
	    {
	      retval = -cb_host_to_target_errno (cb, EINVAL);
	      break;
	    }
	  /* FIXME: Save values for future ugetrlimit calls.  */
	  retval = 0;
	  break;

	/* Provide a very limited subset of the sysctl functions, and
	   abort for the rest. */
	case TARGET_SYS__sysctl:
	  {
	    /* The kernel says:
	       struct __sysctl_args {
		int *name;
		int nlen;
		void *oldval;
		size_t *oldlenp;
		void *newval;
		size_t newlen;
		unsigned long __unused[4];
	       }; */
	    SI name = sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1);
	    SI name0 = name == 0
	      ? 0 : sim_core_read_unaligned_4 (current_cpu, pc, 0, name);
	    SI name1 = name == 0
	      ? 0 : sim_core_read_unaligned_4 (current_cpu, pc, 0, name + 4);
	    SI nlen
	      =  sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1 + 4);
	    SI oldval
	      =  sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1 + 8);
	    SI oldlenp
	      =  sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1 + 12);
	    SI oldlen = oldlenp == 0
	      ? 0 : sim_core_read_unaligned_4 (current_cpu, pc, 0, oldlenp);
	    SI newval
	      =  sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1 + 16);
	    SI newlen
	      = sim_core_read_unaligned_4 (current_cpu, pc, 0, arg1 + 20);

	    if (name0 == TARGET_CTL_KERN && name1 == TARGET_CTL_KERN_VERSION)
	      {
		SI to_write = oldlen < (SI) sizeof (TARGET_UTSNAME)
		  ? oldlen : (SI) sizeof (TARGET_UTSNAME);

		sim_core_write_unaligned_4 (current_cpu, pc, 0, oldlenp,
					    sizeof (TARGET_UTSNAME));

		if (sim_core_write_buffer (sd, current_cpu, write_map,
					   TARGET_UTSNAME, oldval,
					   to_write)
		    != (unsigned int) to_write)
		  retval = -cb_host_to_target_errno (cb, EFAULT);
		else
		  retval = 0;
		break;
	      }

	    retval
	      = cris_unknown_syscall (current_cpu, pc,
				      "Unimplemented _sysctl syscall "
				      "(0x%lx: [0x%lx, 0x%lx],"
				      " 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)\n",
				      (unsigned long) name,
				      (unsigned long) name0,
				      (unsigned long) name1,
				      (unsigned long) nlen,
				      (unsigned long) oldval,
				      (unsigned long) oldlenp,
				      (unsigned long) newval,
				      (unsigned long) newlen);
	    break;
	  }

	case TARGET_SYS_exit:
	  {
	    /* Here for all but the last thread.  */
	    int i;
	    int pid
	      = current_cpu->thread_data[threadno].threadid + TARGET_PID;
	    int ppid
	      = (current_cpu->thread_data[threadno].parent_threadid
		 + TARGET_PID);
	    int exitsig = current_cpu->thread_data[threadno].exitsig;

	    /* Any children are now all orphans.  */
	    for (i = 0; i < SIM_TARGET_MAX_THREADS; i++)
	      if (current_cpu->thread_data[i].parent_threadid
		  == current_cpu->thread_data[threadno].threadid)
		/* Make getppid(2) return 1 for them, poor little ones.  */
		current_cpu->thread_data[i].parent_threadid = -TARGET_PID + 1;

	    /* Free the cpu context data.  When the parent has received
	       the exit status, we'll clear the entry too.  */
	    free (current_cpu->thread_data[threadno].cpu_context);
	    current_cpu->thread_data[threadno].cpu_context = NULL;
	    current_cpu->m1threads--;
	    if (arg1 != 0)
	      {
		sim_io_eprintf (sd, "Thread %d exited with status %d\n",
				pid, arg1);
		sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped,
				 SIM_SIGILL);
	      }

	    /* Still, we may want to support non-zero exit values.  */
	    current_cpu->thread_data[threadno].exitval = arg1 << 8;

	    if (exitsig)
	      deliver_signal (current_cpu, exitsig, ppid);
	    break;
	  }

	case TARGET_SYS_clone:
	  {
	    int nthreads = current_cpu->m1threads + 1;
	    void *thread_cpu_data;
	    bfd_byte old_sp_buf[4];
	    bfd_byte sp_buf[4];
	    const bfd_byte zeros[4] = { 0, 0, 0, 0 };
	    int i;

	    /* That's right, the syscall clone arguments are reversed
	       compared to sys_clone notes in clone(2) and compared to
	       other Linux ports (i.e. it's the same order as in the
	       clone(2) libcall).  */
	    USI flags = arg2;
	    USI newsp = arg1;

	    if (nthreads == SIM_TARGET_MAX_THREADS)
	      {
		retval = -cb_host_to_target_errno (cb, EAGAIN);
		break;
	      }

	    /* FIXME: Implement the low byte.  */
	    if ((flags & ~TARGET_CSIGNAL) !=
		(TARGET_CLONE_VM
		 | TARGET_CLONE_FS
		 | TARGET_CLONE_FILES
		 | TARGET_CLONE_SIGHAND)
		|| newsp == 0)
	      {
		retval
		  = cris_unknown_syscall (current_cpu, pc,
					  "Unimplemented clone syscall "
					  "(0x%lx, 0x%lx)\n",
					  (unsigned long) arg1,
					  (unsigned long) arg2);
		break;
	      }

	    if (current_cpu->thread_data == NULL)
	      make_first_thread (current_cpu);

	    /* The created thread will get the new SP and a cleared R10.
	       Since it's created out of a copy of the old thread and we
	       don't have a set-register-function that just take the
	       cpu_data as a parameter, we set the childs values first,
	       and write back or overwrite them in the parent after the
	       copy.  */
	    (*CPU_REG_FETCH (current_cpu)) (current_cpu,
					    H_GR_SP, old_sp_buf, 4);
	    bfd_putl32 (newsp, sp_buf);
	    (*CPU_REG_STORE (current_cpu)) (current_cpu,
					    H_GR_SP, sp_buf, 4);
	    (*CPU_REG_STORE (current_cpu)) (current_cpu,
					    H_GR_R10, (bfd_byte *) zeros, 4);
	    thread_cpu_data
	      = (*current_cpu
		 ->make_thread_cpu_data) (current_cpu,
					  &current_cpu->cpu_data_placeholder);
	    (*CPU_REG_STORE (current_cpu)) (current_cpu,
					    H_GR_SP, old_sp_buf, 4);

	    retval = ++current_cpu->max_threadid + TARGET_PID;

	    /* Find an unused slot.  After a few threads have been created
	       and exited, the array is expected to be a bit fragmented.
	       We don't reuse the first entry, though, that of the
	       original thread.  */
	    for (i = 1; i < SIM_TARGET_MAX_THREADS; i++)
	      if (current_cpu->thread_data[i].cpu_context == NULL
		  /* Don't reuse a zombied entry.  */
		  && current_cpu->thread_data[i].threadid == 0)
		break;

	    memcpy (&current_cpu->thread_data[i],
		    &current_cpu->thread_data[threadno],
		    sizeof (current_cpu->thread_data[i]));
	    current_cpu->thread_data[i].cpu_context = thread_cpu_data;
	    current_cpu->thread_data[i].cpu_context_atsignal = NULL;
	    current_cpu->thread_data[i].threadid = current_cpu->max_threadid;
	    current_cpu->thread_data[i].parent_threadid
	      = current_cpu->thread_data[threadno].threadid;
	    current_cpu->thread_data[i].pipe_read_fd = 0;
	    current_cpu->thread_data[i].pipe_write_fd = 0;
	    current_cpu->thread_data[i].at_syscall = 0;
	    current_cpu->thread_data[i].sigpending = 0;
	    current_cpu->thread_data[i].sigsuspended = 0;
	    current_cpu->thread_data[i].exitsig = flags & TARGET_CSIGNAL;
	    current_cpu->m1threads = nthreads;
	    break;
	  }

	/* Better watch these in case they do something necessary.  */
	case TARGET_SYS_socketcall:
	  retval = -cb_host_to_target_errno (cb, ENOSYS);
	  break;

	unimplemented_syscall:
	default:
	  retval
	    = cris_unknown_syscall (current_cpu, pc,
				    "Unimplemented syscall: %d "
				    "(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
				    callnum, arg1, arg2, arg3, arg4, arg5,
				    arg6);
	}
    }

  /* Minimal support for fcntl F_GETFL as used in open+fdopen.  */
  if (callnum == TARGET_SYS_open)
    {
      current_cpu->last_open_fd = retval;
      current_cpu->last_open_flags = arg2;
    }

  current_cpu->last_syscall = callnum;

  /* A system call is a rescheduling point.  For the time being, we don't
     reschedule anywhere else.  */
  if (current_cpu->m1threads != 0
      /* We need to schedule off from an exiting thread that is the
	 second-last one.  */
      || (current_cpu->thread_data != NULL
	  && current_cpu->thread_data[threadno].cpu_context == NULL))
    {
      bfd_byte retval_buf[4];

      current_cpu->thread_data[threadno].last_execution
	= TARGET_TIME_MS (current_cpu);
      bfd_putl32 (retval, retval_buf);
      (*CPU_REG_STORE (current_cpu)) (current_cpu, H_GR_R10, retval_buf, 4);

      current_cpu->thread_data[threadno].at_syscall = 1;
      reschedule (current_cpu);

      (*CPU_REG_FETCH (current_cpu)) (current_cpu, H_GR_R10, retval_buf, 4);
      retval = bfd_getl32 (retval_buf);
    }

  return retval;
}

/* Callback from simulator write saying that the pipe at (reader, writer)
   is now non-empty (so the writer should wait until the pipe is empty, at
   least not write to this or any other pipe).  Simplest is to just wait
   until the pipe is empty.  */

static void
cris_pipe_nonempty (host_callback *cb ATTRIBUTE_UNUSED,
		    int reader, int writer)
{
  SIM_CPU *cpu = current_cpu_for_cb_callback;
  const bfd_byte zeros[4] = { 0, 0, 0, 0 };

  /* It's the current thread: we just have to re-run the current
     syscall instruction (presumably "break 13") and change the syscall
     to the special simulator-wait code.  Oh, and set a marker that
     we're waiting, so we can disambiguate the special call from a
     program error.

     This function may be called multiple times between cris_pipe_empty,
     but we must avoid e.g. decreasing PC every time.  Check fd markers
     to tell.  */
  if (cpu->thread_data == NULL)
    {
      sim_io_eprintf (CPU_STATE (cpu),
		      "Terminating simulation due to writing pipe rd:wr %d:%d"
		      " from one single thread\n", reader, writer);
      sim_engine_halt (CPU_STATE (cpu), cpu,
		       NULL, sim_pc_get (cpu), sim_stopped, SIM_SIGILL);
    }
  else if (cpu->thread_data[cpu->threadno].pipe_write_fd == 0)
    {
      cpu->thread_data[cpu->threadno].pipe_write_fd = writer;
      cpu->thread_data[cpu->threadno].pipe_read_fd = reader;
      /* FIXME: We really shouldn't change registers other than R10 in
	 syscalls (like R9), here or elsewhere.  */
      (*CPU_REG_STORE (cpu)) (cpu, H_GR_R9, (bfd_byte *) zeros, 4);
      sim_pc_set (cpu, sim_pc_get (cpu) - 2);
    }
}

/* Callback from simulator close or read call saying that the pipe at
   (reader, writer) is now empty (so the writer can write again, perhaps
   leave a waiting state).  If there are bytes remaining, they couldn't be
   consumed (perhaps due to the pipe closing).  */

static void
cris_pipe_empty (host_callback *cb,
		 int reader,
		 int writer)
{
  int i;
  SIM_CPU *cpu = current_cpu_for_cb_callback;
  bfd_byte r10_buf[4];
  int remaining
    = cb->pipe_buffer[writer].size - cb->pipe_buffer[reader].size;

  /* We need to find the thread that waits for this pipe.  */
  for (i = 0; i < SIM_TARGET_MAX_THREADS; i++)
    if (cpu->thread_data[i].cpu_context
	&& cpu->thread_data[i].pipe_write_fd == writer)
      {
	int retval;

	/* Temporarily switch to this cpu context, so we can change the
	   PC by ordinary calls.  */

	memcpy (cpu->thread_data[cpu->threadno].cpu_context,
		&cpu->cpu_data_placeholder,
		cpu->thread_cpu_data_size);
	memcpy (&cpu->cpu_data_placeholder,
		cpu->thread_data[i].cpu_context,
		cpu->thread_cpu_data_size);

	/* The return value is supposed to contain the number of
	   written bytes, which is the number of bytes requested and
	   returned at the write call.  You might think the right
	   thing is to adjust the return-value to be only the
	   *consumed* number of bytes, but it isn't.  We're only
	   called if the pipe buffer is fully consumed or it is being
	   closed, possibly with remaining bytes.  For the latter
	   case, the writer is still supposed to see success for
	   PIPE_BUF bytes (a constant which we happen to know and is
	   unlikely to change).  The return value may also be a
	   negative number; an error value.  This case is covered
	   because "remaining" is always >= 0.  */
	(*CPU_REG_FETCH (cpu)) (cpu, H_GR_R10, r10_buf, 4);
	retval = (int) bfd_getl_signed_32 (r10_buf);
	if (retval - remaining > TARGET_PIPE_BUF)
	  {
	    bfd_putl32 (retval - remaining, r10_buf);
	    (*CPU_REG_STORE (cpu)) (cpu, H_GR_R10, r10_buf, 4);
	  }
	sim_pc_set (cpu, sim_pc_get (cpu) + 2);
	memcpy (cpu->thread_data[i].cpu_context,
		&cpu->cpu_data_placeholder,
		cpu->thread_cpu_data_size);
	memcpy (&cpu->cpu_data_placeholder,
		cpu->thread_data[cpu->threadno].cpu_context,
		cpu->thread_cpu_data_size);
	cpu->thread_data[i].pipe_read_fd = 0;
	cpu->thread_data[i].pipe_write_fd = 0;
	return;
      }

  abort ();
}

/* We have a simulator-specific notion of time.  See TARGET_TIME.  */

static long
cris_time (host_callback *cb ATTRIBUTE_UNUSED, long *t)
{
  long retval = TARGET_TIME (current_cpu_for_cb_callback);
  if (t)
    *t = retval;
  return retval;
}

/* Set target-specific callback data. */

void
cris_set_callbacks (host_callback *cb)
{
  /* Yeargh, have to cast away constness to avoid warnings.  */
  cb->syscall_map = (CB_TARGET_DEFS_MAP *) syscall_map;
  cb->errno_map = (CB_TARGET_DEFS_MAP *) errno_map;

  /* The kernel stat64 layout.  If we see a file > 2G, the "long"
     parameter to cb_store_target_endian will make st_size negative.
     Similarly for st_ino.  FIXME: Find a 64-bit type, and use it
     *unsigned*, and/or add syntax for signed-ness.  */
  cb->stat_map = stat_map;
  cb->open_map = (CB_TARGET_DEFS_MAP *) open_map;
  cb->pipe_nonempty = cris_pipe_nonempty;
  cb->pipe_empty = cris_pipe_empty;
  cb->time = cris_time;
}

/* Process an address exception.  */

void
cris_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
		  unsigned int map, int nr_bytes, address_word addr,
		  transfer_type transfer, sim_core_signals sig)
{
  sim_core_signal (sd, current_cpu, cia, map, nr_bytes, addr,
		   transfer, sig);
}
