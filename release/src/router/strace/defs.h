/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MIPS
#include <sgidefs.h>
#endif

#ifdef linux
#include <features.h>
#endif

#ifdef _LARGEFILE64_SOURCE
/* This is the macro everything checks before using foo64 names.  */
# ifndef _LFS64_LARGEFILE
#  define _LFS64_LARGEFILE 1
# endif
#endif

/* configuration section */
#ifndef MAX_QUALS
#if defined(LINUX) && defined(MIPS)
#define MAX_QUALS	7000	/* maximum number of syscalls, signals, etc. */
#else
#define MAX_QUALS	2048	/* maximum number of syscalls, signals, etc. */
#endif
#endif
#ifndef DEFAULT_STRLEN
#define DEFAULT_STRLEN	32	/* default maximum # of bytes printed in
				  `printstr', change with `-s' switch */
#endif
#ifndef DEFAULT_ACOLUMN
#define DEFAULT_ACOLUMN	40	/* default alignment column for results */
#endif
#ifndef MAX_ARGS
# ifdef HPPA
#  define MAX_ARGS	6	/* maximum number of args to a syscall */
# else
#  define MAX_ARGS	32	/* maximum number of args to a syscall */
# endif
#endif
#ifndef DEFAULT_SORTBY
#define DEFAULT_SORTBY "time"	/* default sorting method for call profiling */
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif /* STDC_HEADERS */

#ifdef HAVE_SIGINFO_T
#include <signal.h>
#endif

#if defined(LINUX)
#  if defined(SPARC) || defined(SPARC64)
#     define LINUXSPARC
#  endif
#  if defined(X86_64)
#     define LINUX_X86_64
#  endif
#  if defined(MIPS) && _MIPS_SIM == _MIPS_SIM_ABI32
#     define LINUX_MIPSO32
#  endif
#  if defined(MIPS) && _MIPS_SIM == _MIPS_SIM_NABI32
#     define LINUX_MIPSN32
#     define LINUX_MIPS64
#  endif
#  if defined(MIPS) && _MIPS_SIM == _MIPS_SIM_ABI64
#     define LINUX_MIPSN64
#     define LINUX_MIPS64
#  endif
#  if defined(ARM)
#     define LINUX_ARM
#  endif
#  if defined(AVR32)
#     define LINUX_AVR32
#  endif
#endif

#if defined(SVR4) || defined(FREEBSD)
#define USE_PROCFS
#else
#undef USE_PROCFS
#endif

#ifdef FREEBSD
#ifndef I386
#error "FreeBSD support is only for i386 arch right now."
#endif
#include <machine/psl.h>
#include <machine/reg.h>
#include <sys/syscall.h>
#endif

#ifdef USE_PROCFS
#include <sys/procfs.h>
#ifdef HAVE_MP_PROCFS
#include <sys/uio.h>
#endif
#ifdef FREEBSD
#include <sys/pioctl.h>
#endif /* FREEBSD */
#else /* !USE_PROCFS */
#if (defined(LINUXSPARC) || defined(LINUX_X86_64) || defined(LINUX_ARM) || defined(LINUX_AVR32)) && defined(__GLIBC__)
#include <sys/ptrace.h>
#else
/* Work around awkward prototype in ptrace.h. */
#define ptrace xptrace
#include <sys/ptrace.h>
#undef ptrace
#ifdef POWERPC
#define __KERNEL__
#include <asm/ptrace.h>
#undef __KERNEL__
#endif
#ifdef LINUX
extern long ptrace(int, int, char *, long);
#else /* !LINUX */
extern int ptrace(int, int, char *, int, ...);
#endif /* !LINUX */
#endif /* !LINUXSPARC */
#endif /* !SVR4 */

#ifdef LINUX
#if !defined(__GLIBC__)
#define	PTRACE_PEEKUSER	PTRACE_PEEKUSR
#define	PTRACE_POKEUSER	PTRACE_POKEUSR
#endif
#ifdef ALPHA
#  define REG_R0 0
#  define REG_A0 16
#  define REG_A3 19
#  define REG_FP 30
#  define REG_PC 64
#endif /* ALPHA */
#ifdef MIPS
#  define REG_V0 2
#  define REG_A0 4
#  define REG_A3 7
#  define REG_SP 29
#  define REG_EPC 64
#endif /* MIPS */
#ifdef HPPA
#  define PT_GR20 (20*4)
#  define PT_GR26 (26*4)
#  define PT_GR28 (28*4)
#  define PT_IAOQ0 (106*4)
#  define PT_IAOQ1 (107*4)
#endif /* HPPA */
#ifdef SH64
   /* SH64 Linux - this code assumes the following kernel API for system calls:
          PC           Offset 0
          System Call  Offset 16 (actually, (syscall no.) | (0x1n << 16),
                       where n = no. of parameters.
          Other regs   Offset 24+

          On entry:    R2-7 = parameters 1-6 (as many as necessary)
          On return:   R9   = result. */

   /* Offset for peeks of registers */
#  define REG_OFFSET         (24)
#  define REG_GENERAL(x)     (8*(x)+REG_OFFSET)
#  define REG_PC             (0*8)
#  define REG_SYSCALL        (2*8)
#endif /* SH64 */
#endif /* LINUX */

#define SUPPORTED_PERSONALITIES 1
#define DEFAULT_PERSONALITY 0

#ifdef LINUXSPARC
/* Indexes into the pt_regs.u_reg[] array -- UREG_XX from kernel are all off
 * by 1 and use Ix instead of Ox.  These work for both 32 and 64 bit Linux. */
#define U_REG_G1 0
#define U_REG_O0 7
#define U_REG_O1 8
#define PERSONALITY0_WORDSIZE 4
#define PERSONALITY1_WORDSIZE 4
#undef  SUPPORTED_PERSONALITIES
#if defined(SPARC64)
#include <asm/psrcompat.h>
#define SUPPORTED_PERSONALITIES 3
#define PERSONALITY2_WORDSIZE 8
#else
#include <asm/psr.h>
#define SUPPORTED_PERSONALITIES 2
#endif /* SPARC64 */
#endif /* LINUXSPARC */

#ifdef X86_64
#undef SUPPORTED_PERSONALITIES
#define SUPPORTED_PERSONALITIES 2
#define PERSONALITY0_WORDSIZE 8
#define PERSONALITY1_WORDSIZE 4
#endif

#ifdef ARM
#undef SUPPORTED_PERSONALITIES
#define SUPPORTED_PERSONALITIES 2
#define PERSONALITY0_WORDSIZE 4
#define PERSONALITY1_WORDSIZE 4
#endif

#ifdef POWERPC64
#undef SUPPORTED_PERSONALITIES
#define SUPPORTED_PERSONALITIES 2
#define PERSONALITY0_WORDSIZE 8
#define PERSONALITY1_WORDSIZE 4
#endif

#ifdef SVR4
#ifdef HAVE_MP_PROCFS
extern int mp_ioctl (int f, int c, void *a, int s);
#define IOCTL(f,c,a)	mp_ioctl (f, c, a, sizeof *a)
#define IOCTL_STATUS(t) \
	 pread (t->pfd_stat, &t->status, sizeof t->status, 0)
#define IOCTL_WSTOP(t)						\
	(IOCTL (t->pfd, PCWSTOP, (char *)NULL) < 0 ? -1 :		\
	 IOCTL_STATUS (t))
#define PR_WHY		pr_lwp.pr_why
#define PR_WHAT		pr_lwp.pr_what
#define PR_REG		pr_lwp.pr_context.uc_mcontext.gregs
#define PR_FLAGS	pr_lwp.pr_flags
#define PR_SYSCALL	pr_lwp.pr_syscall
#define PR_INFO		pr_lwp.pr_info
#define PIOCSTIP	PCSTOP
#define PIOCSET		PCSET
#define PIOCRESET	PCRESET
#define PIOCSTRACE	PCSTRACE
#define PIOCSFAULT	PCSFAULT
#define PIOCWSTOP	PCWSTOP
#define PIOCSTOP	PCSTOP
#define PIOCSENTRY	PCSENTRY
#define PIOCSEXIT	PCSEXIT
#define PIOCRUN		PCRUN
#else
#define IOCTL		ioctl
#define IOCTL_STATUS(t)	ioctl (t->pfd, PIOCSTATUS, &t->status)
#define IOCTL_WSTOP(t)	ioctl (t->pfd, PIOCWSTOP, &t->status)
#define PR_WHY		pr_why
#define PR_WHAT		pr_what
#define PR_REG		pr_reg
#define PR_FLAGS	pr_flags
#define PR_SYSCALL	pr_syscall
#define PR_INFO		pr_info
#endif
#endif
#ifdef FREEBSD
#define IOCTL		ioctl
#define IOCTL_STATUS(t)	ioctl (t->pfd, PIOCSTATUS, &t->status)
#define IOCTL_WSTOP(t)	ioctl (t->pfd, PIOCWAIT, &t->status)
#define PIOCRUN         PIOCCONT
#define PIOCWSTOP       PIOCWAIT
#define PR_WHY		why
#define PR_WHAT		val
#define PR_FLAGS	state
/* from /usr/src/sys/miscfs/procfs/procfs_vnops.c,
   status.state = 0 for running, 1 for stopped */
#define PR_ASLEEP	1
#define PR_SYSENTRY     S_SCE
#define PR_SYSEXIT      S_SCX
#define PR_SIGNALLED    S_SIG
#define PR_FAULTED      S_CORE
#endif

#ifdef LINUX
# if !HAVE_DECL_PTRACE_SETOPTIONS
#  define PTRACE_SETOPTIONS	0x4200
# endif
# if !HAVE_DECL_PTRACE_GETEVENTMSG
#  define PTRACE_GETEVENTMSG	0x4201
# endif
# if !HAVE_DECL_PTRACE_GETSIGINFO
#  define PTRACE_GETSIGINFO	0x4202
# endif
# if !HAVE_DECL_PTRACE_O_TRACEFORK
#  define PTRACE_O_TRACEFORK	0x00000002
# endif
# if !HAVE_DECL_PTRACE_O_TRACEVFORK
#  define PTRACE_O_TRACEVFORK	0x00000004
# endif
# if !HAVE_DECL_PTRACE_O_TRACECLONE
#  define PTRACE_O_TRACECLONE	0x00000008
# endif

# if !HAVE_DECL_PTRACE_EVENT_FORK
#  define PTRACE_EVENT_FORK	1
# endif
# if !HAVE_DECL_PTRACE_EVENT_VFORK
#  define PTRACE_EVENT_VFORK	2
# endif
# if !HAVE_DECL_PTRACE_EVENT_CLONE
#  define PTRACE_EVENT_CLONE	3
# endif
#endif /* LINUX */

/* Trace Control Block */
struct tcb {
	short flags;		/* See below for TCB_ values */
	int pid;		/* Process Id of this entry */
	long scno;		/* System call number */
	int u_nargs;		/* System call arguments */
	long u_arg[MAX_ARGS];	/* System call arguments */
#if defined (LINUX_MIPSN32)
	long long ext_arg[MAX_ARGS];	/* System call arguments */
#endif
	int u_error;		/* Error code */
	long u_rval;		/* (first) return value */
#ifdef HAVE_LONG_LONG
	long long u_lrval;	/* long long return value */
#endif
	FILE *outf;		/* Output file for this process */
	int curcol;		/* Output column for this process */
	const char *auxstr;	/* Auxiliary info from syscall (see RVAL_STR) */
	struct timeval stime;	/* System time usage as of last process wait */
	struct timeval dtime;	/* Delta for system time usage */
	struct timeval etime;	/* Syscall entry time */
				/* Support for tracing forked processes */
	struct tcb *parent;	/* Parent of this process */
	int nchildren;		/* # of traced children */
	int waitpid;		/* pid(s) this process is waiting for */
	int nzombies;		/* # of formerly traced children now dead */
#ifdef LINUX
	int nclone_threads;	/* # of nchildren with CLONE_THREAD */
	int nclone_waiting;	/* clone threads in wait4 (TCB_SUSPENDED) */
#endif
				/* (1st arg of wait4()) */
	long baddr;		/* `Breakpoint' address */
	long inst[2];		/* Instructions on above */
	int pfd;		/* proc file descriptor */
#ifdef SVR4
#ifdef HAVE_MP_PROCFS
	int pfd_stat;
	int pfd_as;
	pstatus_t status;
#else
	prstatus_t status;	/* procfs status structure */
#endif
#endif
	int ptrace_errno;
#ifdef FREEBSD
	struct procfs_status status;
	int pfd_reg;
	int pfd_status;
#endif
};

/* TCB flags */
#define TCB_STARTUP	00001	/* We have just begun ptracing this process */
#define TCB_INUSE	00002	/* This table entry is in use */
#define TCB_INSYSCALL	00004	/* A system call is in progress */
#define TCB_ATTACHED	00010	/* Process is not our own child */
#define TCB_EXITING	00020	/* As far as we know, this process is exiting */
#define TCB_SUSPENDED	00040	/* Process can not be allowed to resume just now */
#define TCB_BPTSET	00100	/* "Breakpoint" set after fork(2) */
#define TCB_SIGTRAPPED	00200	/* Process wanted to block SIGTRAP */
#define TCB_FOLLOWFORK	00400	/* Process should have forks followed */
#define TCB_REPRINT	01000	/* We should reprint this syscall on exit */
#ifdef LINUX
/* x86 does not need TCB_WAITEXECVE.
 * It can detect execve's SIGTRAP by looking at eax/rax.
 * See "stray syscall exit: eax = " message in syscall_fixup().
 */
# if defined(ALPHA) || defined(AVR32) || defined(SPARC) || defined(SPARC64) \
  || defined(POWERPC) || defined(IA64) || defined(HPPA) \
  || defined(SH) || defined(SH64) || defined(S390) || defined(S390X) \
  || defined(ARM) || defined(MIPS) || defined(BFIN) || defined(TILE)
#  define TCB_WAITEXECVE 02000	/* ignore SIGTRAP after exceve */
# endif
# define TCB_CLONE_THREAD  010000 /* CLONE_THREAD set in creating syscall */
# define TCB_GROUP_EXITING 020000 /* TCB_EXITING was exit_group, not _exit */
# include <sys/syscall.h>
# ifndef __NR_exit_group
# /* Hack: Most headers around are too old to have __NR_exit_group.  */
#  ifdef ALPHA
#   define __NR_exit_group 405
#  elif defined I386
#   define __NR_exit_group 252
#  elif defined X86_64
#   define __NR_exit_group 231
#  elif defined IA64
#   define __NR_exit_group 1236
#  elif defined POWERPC
#   define __NR_exit_group 234
#  elif defined S390 || defined S390X
#   define __NR_exit_group 248
#  elif defined SPARC || defined SPARC64
#   define __NR_exit_group 188
#  elif defined M68K
#   define __NR_exit_group 247
#  endif /* ALPHA et al */
# endif	/* !__NR_exit_group */
#endif /* LINUX */

/* qualifier flags */
#define QUAL_TRACE	0001	/* this system call should be traced */
#define QUAL_ABBREV	0002	/* abbreviate the structures of this syscall */
#define QUAL_VERBOSE	0004	/* decode the structures of this syscall */
#define QUAL_RAW	0010	/* print all args in hex for this syscall */
#define QUAL_SIGNAL	0020	/* report events with this signal */
#define QUAL_FAULT	0040	/* report events with this fault */
#define QUAL_READ	0100	/* dump data read on this file descriptor */
#define QUAL_WRITE	0200	/* dump data written to this file descriptor */

#define entering(tcp)	(!((tcp)->flags & TCB_INSYSCALL))
#define exiting(tcp)	((tcp)->flags & TCB_INSYSCALL)
#define syserror(tcp)	((tcp)->u_error != 0)
#define verbose(tcp)	(qual_flags[(tcp)->scno] & QUAL_VERBOSE)
#define abbrev(tcp)	(qual_flags[(tcp)->scno] & QUAL_ABBREV)

struct xlat {
	int val;
	const char *str;
};

extern const struct xlat open_mode_flags[];
extern const struct xlat addrfams[];
extern const struct xlat struct_user_offsets[];
extern const struct xlat open_access_modes[];

/* Format of syscall return values */
#define RVAL_DECIMAL	000	/* decimal format */
#define RVAL_HEX	001	/* hex format */
#define RVAL_OCTAL	002	/* octal format */
#define RVAL_UDECIMAL	003	/* unsigned decimal format */
#define RVAL_LDECIMAL	004	/* long decimal format */
#define RVAL_LHEX	005	/* long hex format */
#define RVAL_LOCTAL	006	/* long octal format */
#define RVAL_LUDECIMAL	007	/* long unsigned decimal format */
#define RVAL_MASK	007	/* mask for these values */

#define RVAL_STR	010	/* Print `auxstr' field after return val */
#define RVAL_NONE	020	/* Print nothing */

#ifndef offsetof
#define offsetof(type, member)	(((char *) &(((type *) NULL)->member)) - \
				 ((char *) (type *) NULL))
#endif /* !offsetof */

/* get offset of member within a user struct */
#define uoff(member)	offsetof(struct user, member)

#define TRACE_FILE	001	/* Trace file-related syscalls. */
#define TRACE_IPC	002	/* Trace IPC-related syscalls. */
#define TRACE_NETWORK	004	/* Trace network-related syscalls. */
#define TRACE_PROCESS	010	/* Trace process-related syscalls. */
#define TRACE_SIGNAL	020	/* Trace signal-related syscalls. */
#define TRACE_DESC	040	/* Trace file descriptor-related syscalls. */
#define SYSCALL_NEVER_FAILS	0100	/* Syscall is always successful. */

typedef enum {
	CFLAG_NONE = 0,
	CFLAG_ONLY_STATS,
	CFLAG_BOTH
} cflag_t;

extern struct tcb **tcbtab;
extern int *qual_flags;
extern int debug, followfork;
extern unsigned int ptrace_setoptions;
extern int dtime, xflag, qflag;
extern cflag_t cflag;
extern int acolumn;
extern unsigned int nprocs, tcbtabsize;
extern int max_strlen;
extern struct tcb *tcp_last;

enum bitness_t { BITNESS_CURRENT = 0, BITNESS_32 };

extern int set_personality(int personality);
extern const char *xlookup(const struct xlat *, int);
extern struct tcb *alloc_tcb(int, int);
extern struct tcb *pid2tcb(int);
extern void droptcb(struct tcb *);
extern void expand_tcbtab(void);

#define alloctcb(pid)	alloc_tcb((pid), 1)

extern void set_sortby(const char *);
extern void set_overhead(int);
extern void qualify(const char *);
extern int get_scno(struct tcb *);
extern long known_scno(struct tcb *);
extern long do_ptrace(int request, struct tcb *tcp, void *addr, void *data);
extern int ptrace_restart(int request, struct tcb *tcp, int sig);
extern int force_result(struct tcb *, int, long);
extern int trace_syscall(struct tcb *);
extern int count_syscall(struct tcb *, struct timeval *);
extern void printxval(const struct xlat *, int, const char *);
extern int printargs(struct tcb *);
extern int addflags(const struct xlat *, int);
extern int printflags(const struct xlat *, int, const char *);
extern const char *sprintflags(const char *, const struct xlat *, int);
extern int umoven(struct tcb *, long, int, char *);
extern int umovestr(struct tcb *, long, int, char *);
extern int upeek(struct tcb *, long, long *);
extern void dumpiov(struct tcb *, int, long);
extern void dumpstr(struct tcb *, long, int);
extern void printstr(struct tcb *, long, int);
extern void printnum(struct tcb *, long, const char *);
extern void printnum_int(struct tcb *, long, const char *);
extern void printpath(struct tcb *, long);
extern void printpathn(struct tcb *, long, int);
extern void printtv_bitness(struct tcb *, long, enum bitness_t, int);
extern void sprinttv(struct tcb *, long, enum bitness_t, char *);
extern void print_timespec(struct tcb *, long);
extern void sprint_timespec(char *, struct tcb *, long);
#ifdef HAVE_SIGINFO_T
extern void printsiginfo(siginfo_t *, int);
#endif
extern void printfd(struct tcb *, int);
extern void printsock(struct tcb *, long, int);
extern void print_sock_optmgmt(struct tcb *, long, int);
extern void printrusage(struct tcb *, long);
extern void printuid(const char *, unsigned long);
extern int clearbpt(struct tcb *);
extern int setbpt(struct tcb *);
extern int sigishandled(struct tcb *, int);
extern void printcall(struct tcb *);
extern const char *signame(int);
extern void print_sigset(struct tcb *, long, int);
extern void printsignal(int);
extern void printleader(struct tcb *);
extern void printtrailer(void);
extern void tabto(int);
extern void call_summary(FILE *);
extern void tprint_iov(struct tcb *, unsigned long, unsigned long);
extern void tprint_open_modes(mode_t);
extern const char *sprint_open_modes(mode_t);
extern int is_restart_error(struct tcb *);

extern int change_syscall(struct tcb *, int);
extern int internal_fork(struct tcb *);
extern int internal_exec(struct tcb *);
extern int internal_wait(struct tcb *, int);
extern int internal_exit(struct tcb *);
#ifdef LINUX
extern int handle_new_child(struct tcb *, int, int);
#endif

extern const struct ioctlent *ioctl_lookup(long);
extern const struct ioctlent *ioctl_next_match(const struct ioctlent *);
extern int ioctl_decode(struct tcb *, long, long);
extern int term_ioctl(struct tcb *, long, long);
extern int sock_ioctl(struct tcb *, long, long);
extern int proc_ioctl(struct tcb *, int, int);
extern int stream_ioctl(struct tcb *, int, int);
#ifdef LINUX
extern int rtc_ioctl(struct tcb *, long, long);
extern int scsi_ioctl(struct tcb *, long, long);
extern int block_ioctl(struct tcb *, long, long);
#endif

extern int tv_nz(struct timeval *);
extern int tv_cmp(struct timeval *, struct timeval *);
extern double tv_float(struct timeval *);
extern void tv_add(struct timeval *, struct timeval *, struct timeval *);
extern void tv_sub(struct timeval *, struct timeval *, struct timeval *);
extern void tv_mul(struct timeval *, struct timeval *, int);
extern void tv_div(struct timeval *, struct timeval *, int);

#ifdef SUNOS4
extern int fixvfork(struct tcb *);
#endif
#if !(defined(LINUX) && !defined(SPARC) && !defined(SPARC64) && !defined(IA64) \
	&& !defined(SH))
extern long getrval2(struct tcb *);
#endif
#ifdef USE_PROCFS
extern int proc_open(struct tcb *tcp, int attaching);
#endif

#define umove(pid, addr, objp)	\
	umoven((pid), (addr), sizeof *(objp), (char *) (objp))

#define printtv(tcp, addr)	\
	printtv_bitness((tcp), (addr), BITNESS_CURRENT, 0)
#define printtv_special(tcp, addr)	\
	printtv_bitness((tcp), (addr), BITNESS_CURRENT, 1)

extern void tprintf(const char *fmt, ...)
#ifdef __GNUC__
	__attribute__ ((format (printf, 1, 2)))
#endif
	;

#ifndef HAVE_STRERROR
const char *strerror(int);
#endif
#ifndef HAVE_STRSIGNAL
const char *strsignal(int);
#endif

extern int current_personality;
extern const int personality_wordsize[];

struct sysent {
	int	nargs;
	int	sys_flags;
	int	(*sys_func)();
	const char *sys_name;
	long	native_scno;	/* Match against SYS_* constants.  */
};

extern const struct sysent *sysent;
extern int nsyscalls;

extern const char *const *errnoent;
extern int nerrnos;

struct ioctlent {
	const char *doth;
	const char *symbol;
	unsigned long code;
};

extern const struct ioctlent *ioctlent;
extern int nioctlents;

extern const char *const *signalent;
extern int nsignals;

extern const struct ioctlent ioctlent0[];
extern const int nioctlents0;
extern const char *const signalent0[];
extern const int nsignals0;

#if SUPPORTED_PERSONALITIES >= 2
extern const struct ioctlent ioctlent1[];
extern const int nioctlents1;
extern const char *const signalent1[];
extern const int nsignals1;
#endif /* SUPPORTED_PERSONALITIES >= 2 */

#if SUPPORTED_PERSONALITIES >= 3
extern const struct ioctlent ioctlent2[];
extern const int nioctlents2;
extern const char *const signalent2[];
extern const int nsignals2;
#endif /* SUPPORTED_PERSONALITIES >= 3 */

#if HAVE_LONG_LONG

/* _l refers to the lower numbered u_arg,
 * _h refers to the higher numbered u_arg
 */

#if HAVE_LITTLE_ENDIAN_LONG_LONG
#define LONG_LONG(_l,_h) \
    ((long long)((unsigned long long)(unsigned)(_l) | ((unsigned long long)(_h)<<32)))
#else
#define LONG_LONG(_l,_h) \
    ((long long)((unsigned long long)(unsigned)(_h) | ((unsigned long long)(_l)<<32)))
#endif

extern int printllval(struct tcb *, const char *, int);
#endif

#ifdef IA64
extern long ia32;
#endif

extern int not_failing_only;
