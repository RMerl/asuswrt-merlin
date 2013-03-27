/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *                     Linux for s390 port by D.J. Barrow
 *                    <barrow_dj@mail.yahoo.com,djbarrow@de.ibm.com>
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

#include "defs.h"

#include <stdint.h>
#include <signal.h>
#include <sys/user.h>
#include <fcntl.h>

#ifdef SVR4
#include <sys/ucontext.h>
#endif /* SVR4 */

#ifdef HAVE_SYS_REG_H
# include <sys/reg.h>
#ifndef PTRACE_PEEKUSR
# define PTRACE_PEEKUSR PTRACE_PEEKUSER
#endif
#ifndef PTRACE_POKEUSR
# define PTRACE_POKEUSR PTRACE_POKEUSER
#endif
#elif defined(HAVE_LINUX_PTRACE_H)
#undef PTRACE_SYSCALL
# ifdef HAVE_STRUCT_IA64_FPREG
#  define ia64_fpreg XXX_ia64_fpreg
# endif
# ifdef HAVE_STRUCT_PT_ALL_USER_REGS
#  define pt_all_user_regs XXX_pt_all_user_regs
# endif
#include <linux/ptrace.h>
# undef ia64_fpreg
# undef pt_all_user_regs
#endif


#ifdef LINUX

#ifdef IA64
# include <asm/ptrace_offsets.h>
#endif /* !IA64 */

#if defined (LINUX) && defined (SPARC64)
# undef PTRACE_GETREGS
# define PTRACE_GETREGS PTRACE_GETREGS64
# undef PTRACE_SETREGS
# define PTRACE_SETREGS PTRACE_SETREGS64
#endif /* LINUX && SPARC64 */

#if defined (SPARC) || defined (SPARC64) || defined (MIPS)
typedef struct {
	struct pt_regs		si_regs;
	int			si_mask;
} m_siginfo_t;
#elif defined HAVE_ASM_SIGCONTEXT_H
#if !defined(IA64) && !defined(X86_64)
#include <asm/sigcontext.h>
#endif /* !IA64 && !X86_64 */
#else /* !HAVE_ASM_SIGCONTEXT_H */
#if defined I386 && !defined HAVE_STRUCT_SIGCONTEXT_STRUCT
struct sigcontext_struct {
	unsigned short gs, __gsh;
	unsigned short fs, __fsh;
	unsigned short es, __esh;
	unsigned short ds, __dsh;
	unsigned long edi;
	unsigned long esi;
	unsigned long ebp;
	unsigned long esp;
	unsigned long ebx;
	unsigned long edx;
	unsigned long ecx;
	unsigned long eax;
	unsigned long trapno;
	unsigned long err;
	unsigned long eip;
	unsigned short cs, __csh;
	unsigned long eflags;
	unsigned long esp_at_signal;
	unsigned short ss, __ssh;
	unsigned long i387;
	unsigned long oldmask;
	unsigned long cr2;
};
#else /* !I386 */
#if defined M68K && !defined HAVE_STRUCT_SIGCONTEXT
struct sigcontext
{
	unsigned long sc_mask;
	unsigned long sc_usp;
	unsigned long sc_d0;
	unsigned long sc_d1;
	unsigned long sc_a0;
	unsigned long sc_a1;
	unsigned short sc_sr;
	unsigned long sc_pc;
	unsigned short sc_formatvec;
};
#endif /* M68K */
#endif /* !I386 */
#endif /* !HAVE_ASM_SIGCONTEXT_H */
#ifndef NSIG
#define NSIG 32
#endif
#ifdef ARM
#undef NSIG
#define NSIG 32
#endif
#endif /* LINUX */

const char *const signalent0[] = {
#include "signalent.h"
};
const int nsignals0 = sizeof signalent0 / sizeof signalent0[0];

#if SUPPORTED_PERSONALITIES >= 2
const char *const signalent1[] = {
#include "signalent1.h"
};
const int nsignals1 = sizeof signalent1 / sizeof signalent1[0];
#endif /* SUPPORTED_PERSONALITIES >= 2 */

#if SUPPORTED_PERSONALITIES >= 3
const char *const signalent2[] = {
#include "signalent2.h"
};
const int nsignals2 = sizeof signalent2 / sizeof signalent2[0];
#endif /* SUPPORTED_PERSONALITIES >= 3 */

const char *const *signalent;
int nsignals;

#if defined(SUNOS4) || defined(FREEBSD)

static const struct xlat sigvec_flags[] = {
	{ SV_ONSTACK,	"SV_ONSTACK"	},
	{ SV_INTERRUPT,	"SV_INTERRUPT"	},
	{ SV_RESETHAND,	"SV_RESETHAND"	},
	{ SA_NOCLDSTOP,	"SA_NOCLDSTOP"	},
	{ 0,		NULL		},
};

#endif /* SUNOS4 || FREEBSD */

#ifdef HAVE_SIGACTION

#if defined LINUX && (defined I386 || defined X86_64)
/* The libc headers do not define this constant since it should only be
   used by the implementation.  So wwe define it here.  */
# ifndef SA_RESTORER
#  define SA_RESTORER 0x04000000
# endif
#endif

static const struct xlat sigact_flags[] = {
#ifdef SA_RESTORER
	{ SA_RESTORER,	"SA_RESTORER"	},
#endif
#ifdef SA_STACK
	{ SA_STACK,	"SA_STACK"	},
#endif
#ifdef SA_RESTART
	{ SA_RESTART,	"SA_RESTART"	},
#endif
#ifdef SA_INTERRUPT
	{ SA_INTERRUPT,	"SA_INTERRUPT"	},
#endif
#ifdef SA_NODEFER
	{ SA_NODEFER,	"SA_NODEFER"	},
#endif
#if defined SA_NOMASK && SA_NODEFER != SA_NOMASK
	{ SA_NOMASK,	"SA_NOMASK"	},
#endif
#ifdef SA_RESETHAND
	{ SA_RESETHAND,	"SA_RESETHAND"	},
#endif
#if defined SA_ONESHOT && SA_ONESHOT != SA_RESETHAND
	{ SA_ONESHOT,	"SA_ONESHOT"	},
#endif
#ifdef SA_SIGINFO
	{ SA_SIGINFO,	"SA_SIGINFO"	},
#endif
#ifdef SA_RESETHAND
	{ SA_RESETHAND,	"SA_RESETHAND"	},
#endif
#ifdef SA_ONSTACK
	{ SA_ONSTACK,	"SA_ONSTACK"	},
#endif
#ifdef SA_NODEFER
	{ SA_NODEFER,	"SA_NODEFER"	},
#endif
#ifdef SA_NOCLDSTOP
	{ SA_NOCLDSTOP,	"SA_NOCLDSTOP"	},
#endif
#ifdef SA_NOCLDWAIT
	{ SA_NOCLDWAIT,	"SA_NOCLDWAIT"	},
#endif
#ifdef _SA_BSDCALL
	{ _SA_BSDCALL,	"_SA_BSDCALL"	},
#endif
#ifdef SA_NOPTRACE
	{ SA_NOPTRACE,	"SA_NOPTRACE"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat sigprocmaskcmds[] = {
	{ SIG_BLOCK,	"SIG_BLOCK"	},
	{ SIG_UNBLOCK,	"SIG_UNBLOCK"	},
	{ SIG_SETMASK,	"SIG_SETMASK"	},
#ifdef SIG_SETMASK32
	{ SIG_SETMASK32,"SIG_SETMASK32"	},
#endif
	{ 0,		NULL		},
};

#endif /* HAVE_SIGACTION */

/* Anonymous realtime signals. */
/* Under glibc 2.1, SIGRTMIN et al are functions, but __SIGRTMIN is a
   constant.  This is what we want.  Otherwise, just use SIGRTMIN. */
#ifdef SIGRTMIN
#ifndef __SIGRTMIN
#define __SIGRTMIN SIGRTMIN
#define __SIGRTMAX SIGRTMAX /* likewise */
#endif
#endif

const char *
signame(sig)
int sig;
{
	static char buf[30];
	if (sig >= 0 && sig < nsignals) {
		return signalent[sig];
#ifdef SIGRTMIN
	} else if (sig >= __SIGRTMIN && sig <= __SIGRTMAX) {
		sprintf(buf, "SIGRT_%ld", (long)(sig - __SIGRTMIN));
		return buf;
#endif /* SIGRTMIN */
	} else {
		sprintf(buf, "%d", sig);
		return buf;
	}
}

#ifndef UNIXWARE
static void
long_to_sigset(l, s)
long l;
sigset_t *s;
{
	sigemptyset(s);
	*(long *)s = l;
}
#endif

static int
copy_sigset_len(tcp, addr, s, len)
struct tcb *tcp;
long addr;
sigset_t *s;
int len;
{
	if (len > sizeof(*s))
		len = sizeof(*s);
	sigemptyset(s);
	if (umoven(tcp, addr, len, (char *)s) < 0)
		return -1;
	return 0;
}

#ifdef LINUX
/* Original sigset is unsigned long */
#define copy_sigset(tcp, addr, s) copy_sigset_len(tcp, addr, s, sizeof(long))
#else
#define copy_sigset(tcp, addr, s) copy_sigset_len(tcp, addr, s, sizeof(sigset_t))
#endif

static const char *
sprintsigmask(const char *str, sigset_t *mask, int rt)
/* set might include realtime sigs */
{
	int i, nsigs;
	int maxsigs;
	const char *format;
	char *s;
	static char outstr[8 * sizeof(sigset_t) * 8];

	strcpy(outstr, str);
	s = outstr + strlen(outstr);
	nsigs = 0;
	maxsigs = nsignals;
#ifdef __SIGRTMAX
	if (rt)
		maxsigs = __SIGRTMAX; /* instead */
#endif
	for (i = 1; i < maxsigs; i++) {
		if (sigismember(mask, i) == 1)
			nsigs++;
	}
	if (nsigs >= nsignals * 2 / 3) {
		*s++ = '~';
		for (i = 1; i < maxsigs; i++) {
			switch (sigismember(mask, i)) {
			case 1:
				sigdelset(mask, i);
				break;
			case 0:
				sigaddset(mask, i);
				break;
			}
		}
	}
	format = "%s";
	*s++ = '[';
	for (i = 1; i < maxsigs; i++) {
		if (sigismember(mask, i) == 1) {
			/* real-time signals on solaris don't have
			 * signalent entries
			 */
			if (i < nsignals) {
				sprintf(s, format, signalent[i] + 3);
			}
#ifdef SIGRTMIN
			else if (i >= __SIGRTMIN && i <= __SIGRTMAX) {
				char tsig[40];
				sprintf(tsig, "RT_%u", i - __SIGRTMIN);
				sprintf(s, format, tsig);
			}
#endif /* SIGRTMIN */
			else {
				char tsig[32];
				sprintf(tsig, "%u", i);
				sprintf(s, format, tsig);
			}
			s += strlen(s);
			format = " %s";
		}
	}
	*s++ = ']';
	*s = '\0';
	return outstr;
}

static void
printsigmask(mask, rt)
sigset_t *mask;
int rt;
{
	tprintf("%s", sprintsigmask("", mask, rt));
}

void
printsignal(nr)
int nr;
{
	tprintf("%s", signame(nr));
}

void
print_sigset(struct tcb *tcp, long addr, int rt)
{
	sigset_t ss;

	if (!addr)
		tprintf("NULL");
	else if (copy_sigset(tcp, addr, &ss) < 0)
		tprintf("%#lx", addr);
	else
		printsigmask(&ss, rt);
}

#ifdef LINUX

#ifndef ILL_ILLOPC
#define ILL_ILLOPC      1       /* illegal opcode */
#define ILL_ILLOPN      2       /* illegal operand */
#define ILL_ILLADR      3       /* illegal addressing mode */
#define ILL_ILLTRP      4       /* illegal trap */
#define ILL_PRVOPC      5       /* privileged opcode */
#define ILL_PRVREG      6       /* privileged register */
#define ILL_COPROC      7       /* coprocessor error */
#define ILL_BADSTK      8       /* internal stack error */
#define FPE_INTDIV      1       /* integer divide by zero */
#define FPE_INTOVF      2       /* integer overflow */
#define FPE_FLTDIV      3       /* floating point divide by zero */
#define FPE_FLTOVF      4       /* floating point overflow */
#define FPE_FLTUND      5       /* floating point underflow */
#define FPE_FLTRES      6       /* floating point inexact result */
#define FPE_FLTINV      7       /* floating point invalid operation */
#define FPE_FLTSUB      8       /* subscript out of range */
#define SEGV_MAPERR     1       /* address not mapped to object */
#define SEGV_ACCERR     2       /* invalid permissions for mapped object */
#define BUS_ADRALN      1       /* invalid address alignment */
#define BUS_ADRERR      2       /* non-existant physical address */
#define BUS_OBJERR      3       /* object specific hardware error */
#define TRAP_BRKPT      1       /* process breakpoint */
#define TRAP_TRACE      2       /* process trace trap */
#define CLD_EXITED      1       /* child has exited */
#define CLD_KILLED      2       /* child was killed */
#define CLD_DUMPED      3       /* child terminated abnormally */
#define CLD_TRAPPED     4       /* traced child has trapped */
#define CLD_STOPPED     5       /* child has stopped */
#define CLD_CONTINUED   6       /* stopped child has continued */
#define POLL_IN         1       /* data input available */
#define POLL_OUT        2       /* output buffers available */
#define POLL_MSG        3       /* input message available */
#define POLL_ERR        4       /* i/o error */
#define POLL_PRI        5       /* high priority input available */
#define POLL_HUP        6       /* device disconnected */
#define SI_KERNEL	0x80	/* sent by kernel */
#define SI_USER         0       /* sent by kill, sigsend, raise */
#define SI_QUEUE        -1      /* sent by sigqueue */
#define SI_TIMER        -2      /* sent by timer expiration */
#define SI_MESGQ        -3      /* sent by real time mesq state change */
#define SI_ASYNCIO      -4      /* sent by AIO completion */
#define SI_SIGIO	-5	/* sent by SIGIO */
#define SI_TKILL	-6	/* sent by tkill */
#define SI_ASYNCNL	-60     /* sent by asynch name lookup completion */

#define SI_FROMUSER(sip)	((sip)->si_code <= 0)

#endif /* LINUX */

#if __GLIBC_MINOR__ < 1
/* Type for data associated with a signal.  */
typedef union sigval
{
	int sival_int;
	void *sival_ptr;
} sigval_t;

# define __SI_MAX_SIZE     128
# define __SI_PAD_SIZE     ((__SI_MAX_SIZE / sizeof (int)) - 3)

typedef struct siginfo
{
	int si_signo;               /* Signal number.  */
	int si_errno;               /* If non-zero, an errno value associated with
								   this signal, as defined in <errno.h>.  */
	int si_code;                /* Signal code.  */

	union
	{
		int _pad[__SI_PAD_SIZE];

		/* kill().  */
		struct
		{
			__pid_t si_pid;     /* Sending process ID.  */
			__uid_t si_uid;     /* Real user ID of sending process.  */
		} _kill;

		/* POSIX.1b timers.  */
		struct
		{
			unsigned int _timer1;
			unsigned int _timer2;
		} _timer;

		/* POSIX.1b signals.  */
		struct
		{
			__pid_t si_pid;     /* Sending process ID.  */
			__uid_t si_uid;     /* Real user ID of sending process.  */
			sigval_t si_sigval; /* Signal value.  */
		} _rt;

		/* SIGCHLD.  */
		struct
		{
			__pid_t si_pid;     /* Which child.  */
			int si_status;      /* Exit value or signal.  */
			__clock_t si_utime;
			__clock_t si_stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS.  */
		struct
		{
			void *si_addr;      /* Faulting insn/memory ref.  */
		} _sigfault;

		/* SIGPOLL.  */
		struct
		{
			int si_band;        /* Band event for SIGPOLL.  */
			int si_fd;
		} _sigpoll;
	} _sifields;
} siginfo_t;

#define si_pid		_sifields._kill.si_pid
#define si_uid		_sifields._kill.si_uid
#define si_status	_sifields._sigchld.si_status
#define si_utime	_sifields._sigchld.si_utime
#define si_stime	_sifields._sigchld.si_stime
#define si_value	_sifields._rt.si_sigval
#define si_int		_sifields._rt.si_sigval.sival_int
#define si_ptr		_sifields._rt.si_sigval.sival_ptr
#define si_addr		_sifields._sigfault.si_addr
#define si_band		_sifields._sigpoll.si_band
#define si_fd		_sifields._sigpoll.si_fd

#endif

#endif

#if defined (SVR4) || defined (LINUX)

static const struct xlat siginfo_codes[] = {
#ifdef SI_KERNEL
	{ SI_KERNEL,	"SI_KERNEL"	},
#endif
#ifdef SI_USER
	{ SI_USER,	"SI_USER"	},
#endif
#ifdef SI_QUEUE
	{ SI_QUEUE,	"SI_QUEUE"	},
#endif
#ifdef SI_TIMER
	{ SI_TIMER,	"SI_TIMER"	},
#endif
#ifdef SI_MESGQ
	{ SI_MESGQ,	"SI_MESGQ"	},
#endif
#ifdef SI_ASYNCIO
	{ SI_ASYNCIO,	"SI_ASYNCIO"	},
#endif
#ifdef SI_SIGIO
	{ SI_SIGIO,	"SI_SIGIO"	},
#endif
#ifdef SI_TKILL
	{ SI_TKILL,	"SI_TKILL"	},
#endif
#ifdef SI_ASYNCNL
	{ SI_ASYNCNL,	"SI_ASYNCNL"	},
#endif
#ifdef SI_NOINFO
	{ SI_NOINFO,	"SI_NOINFO"	},
#endif
#ifdef SI_LWP
	{ SI_LWP,	"SI_LWP"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat sigill_codes[] = {
	{ ILL_ILLOPC,	"ILL_ILLOPC"	},
	{ ILL_ILLOPN,	"ILL_ILLOPN"	},
	{ ILL_ILLADR,	"ILL_ILLADR"	},
	{ ILL_ILLTRP,	"ILL_ILLTRP"	},
	{ ILL_PRVOPC,	"ILL_PRVOPC"	},
	{ ILL_PRVREG,	"ILL_PRVREG"	},
	{ ILL_COPROC,	"ILL_COPROC"	},
	{ ILL_BADSTK,	"ILL_BADSTK"	},
	{ 0,		NULL		},
};

static const struct xlat sigfpe_codes[] = {
	{ FPE_INTDIV,	"FPE_INTDIV"	},
	{ FPE_INTOVF,	"FPE_INTOVF"	},
	{ FPE_FLTDIV,	"FPE_FLTDIV"	},
	{ FPE_FLTOVF,	"FPE_FLTOVF"	},
	{ FPE_FLTUND,	"FPE_FLTUND"	},
	{ FPE_FLTRES,	"FPE_FLTRES"	},
	{ FPE_FLTINV,	"FPE_FLTINV"	},
	{ FPE_FLTSUB,	"FPE_FLTSUB"	},
	{ 0,		NULL		},
};

static const struct xlat sigtrap_codes[] = {
	{ TRAP_BRKPT,	"TRAP_BRKPT"	},
	{ TRAP_TRACE,	"TRAP_TRACE"	},
	{ 0,		NULL		},
};

static const struct xlat sigchld_codes[] = {
	{ CLD_EXITED,	"CLD_EXITED"	},
	{ CLD_KILLED,	"CLD_KILLED"	},
	{ CLD_DUMPED,	"CLD_DUMPED"	},
	{ CLD_TRAPPED,	"CLD_TRAPPED"	},
	{ CLD_STOPPED,	"CLD_STOPPED"	},
	{ CLD_CONTINUED,"CLD_CONTINUED"	},
	{ 0,		NULL		},
};

static const struct xlat sigpoll_codes[] = {
	{ POLL_IN,	"POLL_IN"	},
	{ POLL_OUT,	"POLL_OUT"	},
	{ POLL_MSG,	"POLL_MSG"	},
	{ POLL_ERR,	"POLL_ERR"	},
	{ POLL_PRI,	"POLL_PRI"	},
	{ POLL_HUP,	"POLL_HUP"	},
	{ 0,		NULL		},
};

static const struct xlat sigprof_codes[] = {
#ifdef PROF_SIG
	{ PROF_SIG,	"PROF_SIG"	},
#endif
	{ 0,		NULL		},
};

#ifdef SIGEMT
static const struct xlat sigemt_codes[] = {
#ifdef EMT_TAGOVF
	{ EMT_TAGOVF,	"EMT_TAGOVF"	},
#endif
	{ 0,		NULL		},
};
#endif

static const struct xlat sigsegv_codes[] = {
	{ SEGV_MAPERR,	"SEGV_MAPERR"	},
	{ SEGV_ACCERR,	"SEGV_ACCERR"	},
	{ 0,		NULL		},
};

static const struct xlat sigbus_codes[] = {
	{ BUS_ADRALN,	"BUS_ADRALN"	},
	{ BUS_ADRERR,	"BUS_ADRERR"	},
	{ BUS_OBJERR,	"BUS_OBJERR"	},
	{ 0,		NULL		},
};

void
printsiginfo(siginfo_t *sip, int verbose)
{
	const char *code;

	if (sip->si_signo == 0) {
		tprintf ("{}");
		return;
	}
	tprintf("{si_signo=");
	printsignal(sip->si_signo);
	code = xlookup(siginfo_codes, sip->si_code);
	if (!code) {
		switch (sip->si_signo) {
		case SIGTRAP:
			code = xlookup(sigtrap_codes, sip->si_code);
			break;
		case SIGCHLD:
			code = xlookup(sigchld_codes, sip->si_code);
			break;
		case SIGPOLL:
			code = xlookup(sigpoll_codes, sip->si_code);
			break;
		case SIGPROF:
			code = xlookup(sigprof_codes, sip->si_code);
			break;
		case SIGILL:
			code = xlookup(sigill_codes, sip->si_code);
			break;
#ifdef SIGEMT
		case SIGEMT:
			code = xlookup(sigemt_codes, sip->si_code);
			break;
#endif
		case SIGFPE:
			code = xlookup(sigfpe_codes, sip->si_code);
			break;
		case SIGSEGV:
			code = xlookup(sigsegv_codes, sip->si_code);
			break;
		case SIGBUS:
			code = xlookup(sigbus_codes, sip->si_code);
			break;
		}
	}
	if (code)
		tprintf(", si_code=%s", code);
	else
		tprintf(", si_code=%#x", sip->si_code);
#ifdef SI_NOINFO
	if (sip->si_code != SI_NOINFO)
#endif
	{
		if (sip->si_errno) {
			if (sip->si_errno < 0 || sip->si_errno >= nerrnos)
				tprintf(", si_errno=%d", sip->si_errno);
			else
				tprintf(", si_errno=%s",
					errnoent[sip->si_errno]);
		}
#ifdef SI_FROMUSER
		if (SI_FROMUSER(sip)) {
			tprintf(", si_pid=%lu, si_uid=%lu",
				(unsigned long) sip->si_pid,
				(unsigned long) sip->si_uid);
			switch (sip->si_code) {
#ifdef SI_USER
			case SI_USER:
				break;
#endif
#ifdef SI_TKILL
			case SI_TKILL:
				break;
#endif
#ifdef SI_TIMER
			case SI_TIMER:
				tprintf(", si_value=%d", sip->si_int);
				break;
#endif
#ifdef LINUX
			default:
				if (!sip->si_ptr)
					break;
				if (!verbose)
					tprintf(", ...");
				else
					tprintf(", si_value={int=%u, ptr=%#lx}",
						sip->si_int,
						(unsigned long) sip->si_ptr);
				break;
#endif
			}
		}
		else
#endif /* SI_FROMUSER */
		{
			switch (sip->si_signo) {
			case SIGCHLD:
				tprintf(", si_pid=%ld, si_status=",
					(long) sip->si_pid);
				if (sip->si_code == CLD_EXITED)
					tprintf("%d", sip->si_status);
				else
					printsignal(sip->si_status);
#if LINUX
				if (!verbose)
					tprintf(", ...");
				else
					tprintf(", si_utime=%lu, si_stime=%lu",
						sip->si_utime,
						sip->si_stime);
#endif
				break;
			case SIGILL: case SIGFPE:
			case SIGSEGV: case SIGBUS:
				tprintf(", si_addr=%#lx",
					(unsigned long) sip->si_addr);
				break;
			case SIGPOLL:
				switch (sip->si_code) {
				case POLL_IN: case POLL_OUT: case POLL_MSG:
					tprintf(", si_band=%ld",
						(long) sip->si_band);
					break;
				}
				break;
#ifdef LINUX
			default:
				if (sip->si_pid || sip->si_uid)
				        tprintf(", si_pid=%lu, si_uid=%lu",
						(unsigned long) sip->si_pid,
						(unsigned long) sip->si_uid);
				if (!sip->si_ptr)
					break;
				if (!verbose)
					tprintf(", ...");
				else {
					tprintf(", si_value={int=%u, ptr=%#lx}",
						sip->si_int,
						(unsigned long) sip->si_ptr);
				}
#endif

			}
		}
	}
	tprintf("}");
}

#endif /* SVR4 || LINUX */

#ifdef LINUX

static void
parse_sigset_t(const char *str, sigset_t *set)
{
	const char *p;
	unsigned int digit;
	int i;

	sigemptyset(set);

	p = strchr(str, '\n');
	if (p == NULL)
		p = strchr(str, '\0');
	for (i = 0; p-- > str; i += 4) {
		if (*p >= '0' && *p <= '9')
			digit = *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			digit = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			digit = *p - 'A' + 10;
		else
			break;
		if (digit & 1)
			sigaddset(set, i + 1);
		if (digit & 2)
			sigaddset(set, i + 2);
		if (digit & 4)
			sigaddset(set, i + 3);
		if (digit & 8)
			sigaddset(set, i + 4);
	}
}

#endif

/*
 * Check process TCP for the disposition of signal SIG.
 * Return 1 if the process would somehow manage to  survive signal SIG,
 * else return 0.  This routine will never be called with SIGKILL.
 */
int
sigishandled(tcp, sig)
struct tcb *tcp;
int sig;
{
#ifdef LINUX
	int sfd;
	char sname[32];
	char buf[2048];
	const char *s;
	int i;
	sigset_t ignored, caught;
#endif
#ifdef SVR4
	/*
	 * Since procfs doesn't interfere with wait I think it is safe
	 * to punt on this question.  If not, the information is there.
	 */
	return 1;
#else /* !SVR4 */
	switch (sig) {
	case SIGCONT:
	case SIGSTOP:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
	case SIGCHLD:
	case SIGIO:
#if defined(SIGURG) && SIGURG != SIGIO
	case SIGURG:
#endif
	case SIGWINCH:
		/* Gloria Gaynor says ... */
		return 1;
	default:
		break;
	}
#endif /* !SVR4 */
#ifdef LINUX

	/* This is incredibly costly but it's worth it. */
	/* NOTE: LinuxThreads internally uses SIGRTMIN, SIGRTMIN + 1 and
	   SIGRTMIN + 2, so we can't use the obsolete /proc/%d/stat which
	   doesn't handle real-time signals). */
	sprintf(sname, "/proc/%d/status", tcp->pid);
	if ((sfd = open(sname, O_RDONLY)) == -1) {
		perror(sname);
		return 1;
	}
	i = read(sfd, buf, sizeof(buf));
	buf[i] = '\0';
	close(sfd);
	/*
	 * Skip the extraneous fields. We need to skip
	 * command name has any spaces in it.  So be it.
	 */
	s = strstr(buf, "SigIgn:\t");
	if (!s)
	{
		fprintf(stderr, "/proc/pid/status format error\n");
		return 1;
	}
	parse_sigset_t(s + 8, &ignored);

	s = strstr(buf, "SigCgt:\t");
	if (!s)
	{
		fprintf(stderr, "/proc/pid/status format error\n");
		return 1;
	}
	parse_sigset_t(s + 8, &caught);

#ifdef DEBUG
	fprintf(stderr, "sigs: %016qx %016qx (sig=%d)\n",
		*(long long *) &ignored, *(long long *) &caught, sig);
#endif
	if (sigismember(&ignored, sig) || sigismember(&caught, sig))
		return 1;
#endif /* LINUX */

#ifdef SUNOS4
	void (*u_signal)();

	if (upeek(tcp, uoff(u_signal[0]) + sig*sizeof(u_signal),
	    (long *) &u_signal) < 0) {
		return 0;
	}
	if (u_signal != SIG_DFL)
		return 1;
#endif /* SUNOS4 */

	return 0;
}

#if defined(SUNOS4) || defined(FREEBSD)

int
sys_sigvec(tcp)
struct tcb *tcp;
{
	struct sigvec sv;
	long addr;

	if (entering(tcp)) {
		printsignal(tcp->u_arg[0]);
		tprintf(", ");
		addr = tcp->u_arg[1];
	} else {
		addr = tcp->u_arg[2];
	}
	if (addr == 0)
		tprintf("NULL");
	else if (!verbose(tcp))
		tprintf("%#lx", addr);
	else if (umove(tcp, addr, &sv) < 0)
		tprintf("{...}");
	else {
		switch ((int) sv.sv_handler) {
		case (int) SIG_ERR:
			tprintf("{SIG_ERR}");
			break;
		case (int) SIG_DFL:
			tprintf("{SIG_DFL}");
			break;
		case (int) SIG_IGN:
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
			tprintf("{SIG_IGN}");
			break;
		case (int) SIG_HOLD:
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
			tprintf("SIG_HOLD");
			break;
		default:
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
			tprintf("{%#lx, ", (unsigned long) sv.sv_handler);
			printsigmask(&sv.sv_mask, 0);
			tprintf(", ");
			printflags(sigvec_flags, sv.sv_flags, "SV_???");
			tprintf("}");
		}
	}
	if (entering(tcp))
		tprintf(", ");
	return 0;
}

int
sys_sigpause(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {	/* WTA: UD had a bug here: he forgot the braces */
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[0], &sigm);
		printsigmask(&sigm, 0);
	}
	return 0;
}

int
sys_sigstack(tcp)
struct tcb *tcp;
{
	struct sigstack ss;
	long addr;

	if (entering(tcp))
		addr = tcp->u_arg[0];
	else
		addr = tcp->u_arg[1];
	if (addr == 0)
		tprintf("NULL");
	else if (umove(tcp, addr, &ss) < 0)
		tprintf("%#lx", addr);
	else {
		tprintf("{ss_sp %#lx ", (unsigned long) ss.ss_sp);
		tprintf("ss_onstack %s}", ss.ss_onstack ? "YES" : "NO");
	}
	if (entering(tcp))
		tprintf(", ");
	return 0;
}

int
sys_sigcleanup(tcp)
struct tcb *tcp;
{
	return 0;
}

#endif /* SUNOS4 || FREEBSD */

#ifndef SVR4

int
sys_sigsetmask(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[0], &sigm);
		printsigmask(&sigm, 0);
#ifndef USE_PROCFS
		if ((tcp->u_arg[0] & sigmask(SIGTRAP))) {
			/* Mark attempt to block SIGTRAP */
			tcp->flags |= TCB_SIGTRAPPED;
			/* Send unblockable signal */
			kill(tcp->pid, SIGSTOP);
		}
#endif /* !USE_PROCFS */
	}
	else if (!syserror(tcp)) {
		sigset_t sigm;
		long_to_sigset(tcp->u_rval, &sigm);
		tcp->auxstr = sprintsigmask("old mask ", &sigm, 0);

		return RVAL_HEX | RVAL_STR;
	}
	return 0;
}

#if defined(SUNOS4) || defined(FREEBSD)
int
sys_sigblock(tcp)
struct tcb *tcp;
{
	return sys_sigsetmask(tcp);
}
#endif /* SUNOS4 || FREEBSD */

#endif /* !SVR4 */

#ifdef HAVE_SIGACTION

#ifdef LINUX
struct old_sigaction {
	__sighandler_t __sa_handler;
	unsigned long sa_mask;
	unsigned long sa_flags;
	void (*sa_restorer)(void);
};
#define SA_HANDLER __sa_handler
#endif /* LINUX */

#ifndef SA_HANDLER
#define SA_HANDLER sa_handler
#endif

int
sys_sigaction(tcp)
struct tcb *tcp;
{
	long addr;
#ifdef LINUX
	sigset_t sigset;
	struct old_sigaction sa;
#else
	struct sigaction sa;
#endif


	if (entering(tcp)) {
		printsignal(tcp->u_arg[0]);
		tprintf(", ");
		addr = tcp->u_arg[1];
	} else
		addr = tcp->u_arg[2];
	if (addr == 0)
		tprintf("NULL");
	else if (!verbose(tcp))
		tprintf("%#lx", addr);
	else if (umove(tcp, addr, &sa) < 0)
		tprintf("{...}");
	else {
		/* Architectures using function pointers, like
		 * hppa, may need to manipulate the function pointer
		 * to compute the result of a comparison. However,
		 * the SA_HANDLER function pointer exists only in
		 * the address space of the traced process, and can't
		 * be manipulated by strace. In order to prevent the
		 * compiler from generating code to manipulate
		 * SA_HANDLER we cast the function pointers to long. */
		if ((long)sa.SA_HANDLER == (long)SIG_ERR)
			tprintf("{SIG_ERR, ");
		else if ((long)sa.SA_HANDLER == (long)SIG_DFL)
			tprintf("{SIG_DFL, ");
		else if ((long)sa.SA_HANDLER == (long)SIG_IGN) {
#ifndef USE_PROCFS
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
#endif /* !USE_PROCFS */
			tprintf("{SIG_IGN, ");
		}
		else {
#ifndef USE_PROCFS
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
#endif /* !USE_PROCFS */
			tprintf("{%#lx, ", (long) sa.SA_HANDLER);
#ifndef LINUX
			printsigmask (&sa.sa_mask, 0);
#else
			long_to_sigset(sa.sa_mask, &sigset);
			printsigmask(&sigset, 0);
#endif
			tprintf(", ");
			printflags(sigact_flags, sa.sa_flags, "SA_???");
#ifdef SA_RESTORER
			if (sa.sa_flags & SA_RESTORER)
				tprintf(", %p", sa.sa_restorer);
#endif
			tprintf("}");
		}
	}
	if (entering(tcp))
		tprintf(", ");
#ifdef LINUX
	else
		tprintf(", %#lx", (unsigned long) sa.sa_restorer);
#endif
	return 0;
}

int
sys_signal(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printsignal(tcp->u_arg[0]);
		tprintf(", ");
		switch (tcp->u_arg[1]) {
		case (long) SIG_ERR:
			tprintf("SIG_ERR");
			break;
		case (long) SIG_DFL:
			tprintf("SIG_DFL");
			break;
		case (long) SIG_IGN:
#ifndef USE_PROCFS
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
#endif /* !USE_PROCFS */
			tprintf("SIG_IGN");
			break;
		default:
#ifndef USE_PROCFS
			if (tcp->u_arg[0] == SIGTRAP) {
				tcp->flags |= TCB_SIGTRAPPED;
				kill(tcp->pid, SIGSTOP);
			}
#endif /* !USE_PROCFS */
			tprintf("%#lx", tcp->u_arg[1]);
		}
		return 0;
	}
	else if (!syserror(tcp)) {
		switch (tcp->u_rval) {
		    case (long) SIG_ERR:
			tcp->auxstr = "SIG_ERR"; break;
		    case (long) SIG_DFL:
			tcp->auxstr = "SIG_DFL"; break;
		    case (long) SIG_IGN:
			tcp->auxstr = "SIG_IGN"; break;
		    default:
			tcp->auxstr = NULL;
		}
		return RVAL_HEX | RVAL_STR;
	}
	return 0;
}

#ifdef SVR4
int
sys_sighold(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printsignal(tcp->u_arg[0]);
	}
	return 0;
}
#endif /* SVR4 */

#endif /* HAVE_SIGACTION */

#ifdef LINUX

int
sys_sigreturn(struct tcb *tcp)
{
#if defined(ARM)
	struct pt_regs regs;
	struct sigcontext_struct sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;

		if (ptrace(PTRACE_GETREGS, tcp->pid, NULL, (void *)&regs) == -1)
			return 0;

		if (umove(tcp, regs.ARM_sp, &sc) < 0)
			return 0;

		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.oldmask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(S390) || defined(S390X)
	long usp;
	struct sigcontext_struct sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp,PT_GPR15,&usp)<0)
			return 0;
		if (umove(tcp, usp+__SIGNAL_FRAMESIZE, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		memcpy(&tcp->u_arg[1],&sc.oldmask[0],sizeof(sigset_t));
	} else {
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ",(sigset_t *)&tcp->u_arg[1],0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(I386)
	long esp;
	struct sigcontext_struct sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, 4*UESP, &esp) < 0)
			return 0;
		if (umove(tcp, esp, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.oldmask;
	}
	else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(IA64)
	struct sigcontext sc;
	long sp;

	if (entering(tcp)) {
		/* offset of sigcontext in the kernel's sigframe structure: */
#		define SIGFRAME_SC_OFFSET	0x90
		tcp->u_arg[0] = 0;
		if (upeek(tcp, PT_R12, &sp) < 0)
			return 0;
		if (umove(tcp, sp + 16 + SIGFRAME_SC_OFFSET, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		memcpy(tcp->u_arg + 1, &sc.sc_mask, sizeof(sc.sc_mask));
	}
	else {
		sigset_t sigm;

		memcpy(&sigm, tcp->u_arg + 1, sizeof (sigm));
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(POWERPC)
	long esp;
	struct sigcontext_struct sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, sizeof(unsigned long)*PT_R1, &esp) < 0)
			return 0;
		/* Skip dummy stack frame. */
#ifdef POWERPC64
		if (current_personality == 0)
			esp += 128;
		else
			esp += 64;
#else
		esp += 64;
#endif
		if (umove(tcp, esp, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.oldmask;
	}
	else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(M68K)
	long usp;
	struct sigcontext sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, 4*PT_USP, &usp) < 0)
			return 0;
		if (umove(tcp, usp, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.sc_mask;
	}
	else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(ALPHA)
	long fp;
	struct sigcontext_struct sc;

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, REG_FP, &fp) < 0)
			return 0;
		if (umove(tcp, fp, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.sc_mask;
	}
	else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined (SPARC) || defined (SPARC64)
	long i1;
	struct pt_regs regs;
	m_siginfo_t si;

	if(ptrace(PTRACE_GETREGS, tcp->pid, (char *)&regs, 0) < 0) {
		perror("sigreturn: PTRACE_GETREGS ");
		return 0;
	}
	if(entering(tcp)) {
		tcp->u_arg[0] = 0;
		i1 = regs.u_regs[U_REG_O1];
		if(umove(tcp, i1, &si) < 0) {
			perror("sigreturn: umove ");
			return 0;
		}
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = si.si_mask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if(tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined (LINUX_MIPSN32) || defined (LINUX_MIPSN64)
	/* This decodes rt_sigreturn.  The 64-bit ABIs do not have
	   sigreturn.  */
	long sp;
	struct ucontext uc;

	if(entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, REG_SP, &sp) < 0)
			return 0;
		/* There are six words followed by a 128-byte siginfo.  */
		sp = sp + 6 * 4 + 128;
		if (umove(tcp, sp, &uc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = *(long *) &uc.uc_sigmask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if(tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(MIPS)
	long sp;
	struct pt_regs regs;
	m_siginfo_t si;

	if(ptrace(PTRACE_GETREGS, tcp->pid, (char *)&regs, 0) < 0) {
		perror("sigreturn: PTRACE_GETREGS ");
		return 0;
	}
	if(entering(tcp)) {
		tcp->u_arg[0] = 0;
		sp = regs.regs[29];
		if (umove(tcp, sp, &si) < 0)
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = si.si_mask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if(tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(CRISV10) || defined(CRISV32)
	struct sigcontext sc;

	if (entering(tcp)) {
		long regs[PT_MAX+1];

		tcp->u_arg[0] = 0;

		if (ptrace(PTRACE_GETREGS, tcp->pid, NULL, (long)regs) < 0) {
			perror("sigreturn: PTRACE_GETREGS");
			return 0;
		}
		if (umove(tcp, regs[PT_USP], &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.oldmask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;

		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(TILE)
	struct ucontext uc;
	long sp;

	/* offset of ucontext in the kernel's sigframe structure */
#	define SIGFRAME_UC_OFFSET C_ABI_SAVE_AREA_SIZE + sizeof(struct siginfo)

	if (entering(tcp)) {
		tcp->u_arg[0] = 0;
		if (upeek(tcp, PTREGS_OFFSET_SP, &sp) < 0)
			return 0;
		if (umove(tcp, sp + SIGFRAME_UC_OFFSET, &uc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		memcpy(tcp->u_arg + 1, &uc.uc_sigmask, sizeof(uc.uc_sigmask));
	}
	else {
		sigset_t sigm;

		memcpy(&sigm, tcp->u_arg + 1, sizeof (sigm));
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#elif defined(MICROBLAZE)
	struct sigcontext sc;

	/* TODO: Verify that this is correct...  */
	if (entering(tcp)) {
		long sp;

		tcp->u_arg[0] = 0;

		/* Read r1, the stack pointer.  */
		if (upeek(tcp, 1 * 4, &sp) < 0)
			return 0;
		if (umove(tcp, sp, &sc) < 0)
			return 0;
		tcp->u_arg[0] = 1;
		tcp->u_arg[1] = sc.oldmask;
	} else {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[1], &sigm);
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		tcp->auxstr = sprintsigmask("mask now ", &sigm, 0);
		return RVAL_NONE | RVAL_STR;
	}
	return 0;
#else
#warning No sys_sigreturn() for this architecture
#warning         (no problem, just a reminder :-)
	return 0;
#endif
}

int
sys_siggetmask(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		sigset_t sigm;
		long_to_sigset(tcp->u_rval, &sigm);
		tcp->auxstr = sprintsigmask("mask ", &sigm, 0);
	}
	return RVAL_HEX | RVAL_STR;
}

int
sys_sigsuspend(struct tcb *tcp)
{
	if (entering(tcp)) {
		sigset_t sigm;
		long_to_sigset(tcp->u_arg[2], &sigm);
		printsigmask(&sigm, 0);
	}
	return 0;
}

#endif /* LINUX */

#if defined(SVR4) || defined(FREEBSD)

int
sys_sigsuspend(tcp)
struct tcb *tcp;
{
	sigset_t sigset;

	if (entering(tcp)) {
		if (umove(tcp, tcp->u_arg[0], &sigset) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 0);
	}
	return 0;
}
#ifndef FREEBSD
static const struct xlat ucontext_flags[] = {
	{ UC_SIGMASK,	"UC_SIGMASK"	},
	{ UC_STACK,	"UC_STACK"	},
	{ UC_CPU,	"UC_CPU"	},
#ifdef UC_FPU
	{ UC_FPU,	"UC_FPU"	},
#endif
#ifdef UC_INTR
	{ UC_INTR,	"UC_INTR"	},
#endif
	{ 0,		NULL		},
};
#endif /* !FREEBSD */
#endif /* SVR4 || FREEBSD */

#if defined SVR4 || defined LINUX || defined FREEBSD
#if defined LINUX && !defined SS_ONSTACK
#define SS_ONSTACK      1
#define SS_DISABLE      2
#if __GLIBC_MINOR__ == 0
typedef struct
{
	__ptr_t ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;
#endif
#endif
#ifdef FREEBSD
#define stack_t struct sigaltstack
#endif

static const struct xlat sigaltstack_flags[] = {
	{ SS_ONSTACK,	"SS_ONSTACK"	},
	{ SS_DISABLE,	"SS_DISABLE"	},
	{ 0,		NULL		},
};
#endif

#ifdef SVR4
static void
printcontext(tcp, ucp)
struct tcb *tcp;
ucontext_t *ucp;
{
	tprintf("{");
	if (!abbrev(tcp)) {
		tprintf("uc_flags=");
		printflags(ucontext_flags, ucp->uc_flags, "UC_???");
		tprintf(", uc_link=%#lx, ", (unsigned long) ucp->uc_link);
	}
	tprintf("uc_sigmask=");
	printsigmask(&ucp->uc_sigmask, 0);
	if (!abbrev(tcp)) {
		tprintf(", uc_stack={ss_sp=%#lx, ss_size=%d, ss_flags=",
			(unsigned long) ucp->uc_stack.ss_sp,
			ucp->uc_stack.ss_size);
		printflags(sigaltstack_flags, ucp->uc_stack.ss_flags, "SS_???");
		tprintf("}");
	}
	tprintf(", ...}");
}

int
sys_getcontext(tcp)
struct tcb *tcp;
{
	ucontext_t uc;

	if (exiting(tcp)) {
		if (tcp->u_error)
			tprintf("%#lx", tcp->u_arg[0]);
		else if (!tcp->u_arg[0])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[0], &uc) < 0)
			tprintf("{...}");
		else
			printcontext(tcp, &uc);
	}
	return 0;
}

int
sys_setcontext(tcp)
struct tcb *tcp;
{
	ucontext_t uc;

	if (entering(tcp)) {
		if (!tcp->u_arg[0])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[0], &uc) < 0)
			tprintf("{...}");
		else
			printcontext(tcp, &uc);
	}
	else {
		tcp->u_rval = tcp->u_error = 0;
		if (tcp->u_arg[0] == 0)
			return 0;
		return RVAL_NONE;
	}
	return 0;
}

#endif /* SVR4 */

#if defined(LINUX) || defined(FREEBSD)

static int
print_stack_t(tcp, addr)
struct tcb *tcp;
unsigned long addr;
{
	stack_t ss;
	if (umove(tcp, addr, &ss) < 0)
		return -1;
	tprintf("{ss_sp=%#lx, ss_flags=", (unsigned long) ss.ss_sp);
	printflags(sigaltstack_flags, ss.ss_flags, "SS_???");
	tprintf(", ss_size=%lu}", (unsigned long) ss.ss_size);
	return 0;
}

int
sys_sigaltstack(tcp)
	struct tcb *tcp;
{
	if (entering(tcp)) {
		if (tcp->u_arg[0] == 0)
			tprintf("NULL");
		else if (print_stack_t(tcp, tcp->u_arg[0]) < 0)
			return -1;
	}
	else {
		tprintf(", ");
		if (tcp->u_arg[1] == 0)
			tprintf("NULL");
		else if (print_stack_t(tcp, tcp->u_arg[1]) < 0)
			return -1;
	}
	return 0;
}
#endif

#ifdef HAVE_SIGACTION

int
sys_sigprocmask(tcp)
struct tcb *tcp;
{
#ifdef ALPHA
	if (entering(tcp)) {
		printxval(sigprocmaskcmds, tcp->u_arg[0], "SIG_???");
		tprintf(", ");
		printsigmask(tcp->u_arg[1], 0);
	}
	else if (!syserror(tcp)) {
		tcp->auxstr = sprintsigmask("old mask ", tcp->u_rval, 0);
		return RVAL_HEX | RVAL_STR;
	}
#else /* !ALPHA */
	if (entering(tcp)) {
#ifdef SVR4
		if (tcp->u_arg[0] == 0)
			tprintf("0");
		else
#endif /* SVR4 */
		printxval(sigprocmaskcmds, tcp->u_arg[0], "SIG_???");
		tprintf(", ");
		print_sigset(tcp, tcp->u_arg[1], 0);
		tprintf(", ");
	}
	else {
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[2]);
		else
			print_sigset(tcp, tcp->u_arg[2], 0);
	}
#endif /* !ALPHA */
	return 0;
}

#endif /* HAVE_SIGACTION */

int
sys_kill(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		/*
		 * Sign-extend a 32-bit value when that's what it is.
		 */
		long pid = tcp->u_arg[0];
		if (personality_wordsize[current_personality] < sizeof pid)
			pid = (long) (int) pid;
		tprintf("%ld, %s", pid, signame(tcp->u_arg[1]));
	}
	return 0;
}

#if defined(FREEBSD) || defined(SUNOS4)
int
sys_killpg(tcp)
struct tcb *tcp;
{
	return sys_kill(tcp);
}
#endif /* FREEBSD || SUNOS4 */

#ifdef LINUX
int
sys_tgkill(tcp)
	struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, %ld, %s",
			tcp->u_arg[0], tcp->u_arg[1], signame(tcp->u_arg[2]));
	}
	return 0;
}
#endif

int
sys_sigpending(tcp)
struct tcb *tcp;
{
	sigset_t sigset;

	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else if (copy_sigset(tcp, tcp->u_arg[0], &sigset) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 0);
	}
	return 0;
}

#ifdef SVR4
int sys_sigwait(tcp)
struct tcb *tcp;
{
	sigset_t sigset;

	if (entering(tcp)) {
		if (copy_sigset(tcp, tcp->u_arg[0], &sigset) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 0);
	}
	else {
		if (!syserror(tcp)) {
			tcp->auxstr = signalent[tcp->u_rval];
			return RVAL_DECIMAL | RVAL_STR;
		}
	}
	return 0;
}
#endif /* SVR4 */

#ifdef LINUX

	int
sys_rt_sigprocmask(tcp)
	struct tcb *tcp;
{
	sigset_t sigset;

	/* Note: arg[3] is the length of the sigset. */
	if (entering(tcp)) {
		printxval(sigprocmaskcmds, tcp->u_arg[0], "SIG_???");
		tprintf(", ");
		if (!tcp->u_arg[1])
			tprintf("NULL, ");
		else if (copy_sigset_len(tcp, tcp->u_arg[1], &sigset, tcp->u_arg[3]) < 0)
			tprintf("%#lx, ", tcp->u_arg[1]);
		else {
			printsigmask(&sigset, 1);
			tprintf(", ");
		}
	}
	else {
		if (!tcp->u_arg[2])

			tprintf("NULL");
		else if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[2]);
		else if (copy_sigset_len(tcp, tcp->u_arg[2], &sigset, tcp->u_arg[3]) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 1);
		tprintf(", %lu", tcp->u_arg[3]);
	}
	return 0;
}


/* Structure describing the action to be taken when a signal arrives.  */
struct new_sigaction
{
	__sighandler_t __sa_handler;
	unsigned long sa_flags;
	void (*sa_restorer) (void);
	/* Kernel treats sa_mask as an array of longs. */
	unsigned long sa_mask[NSIG / sizeof(long) ? NSIG / sizeof(long) : 1];
};
/* Same for i386-on-x86_64 and similar cases */
struct new_sigaction32
{
	uint32_t __sa_handler;
	uint32_t sa_flags;
	uint32_t sa_restorer;
	uint32_t sa_mask[2 * (NSIG / sizeof(long) ? NSIG / sizeof(long) : 1)];
};


int
sys_rt_sigaction(struct tcb *tcp)
{
	struct new_sigaction sa;
	sigset_t sigset;
	long addr;
	int r;

	if (entering(tcp)) {
		printsignal(tcp->u_arg[0]);
		tprintf(", ");
		addr = tcp->u_arg[1];
	} else
		addr = tcp->u_arg[2];

	if (addr == 0) {
		tprintf("NULL");
		goto after_sa;
	}
	if (!verbose(tcp)) {
		tprintf("%#lx", addr);
		goto after_sa;
	}
#if SUPPORTED_PERSONALITIES > 1
	if (personality_wordsize[current_personality] != sizeof(sa.sa_flags)
	 && personality_wordsize[current_personality] == 4
	) {
		struct new_sigaction32 sa32;
		r = umove(tcp, addr, &sa32);
		if (r >= 0) {
			memset(&sa, 0, sizeof(sa));
			sa.__sa_handler = (void*)(unsigned long)sa32.__sa_handler;
			sa.sa_flags     = sa32.sa_flags;
			sa.sa_restorer  = (void*)(unsigned long)sa32.sa_restorer;
			/* Kernel treats sa_mask as an array of longs.
			 * For 32-bit process, "long" is uint32_t, thus, for example,
			 * 32th bit in sa_mask will end up as bit 0 in sa_mask[1].
			 * But for (64-bit) kernel, 32th bit in sa_mask is
			 * 32th bit in 0th (64-bit) long!
			 * For little-endian, it's the same.
			 * For big-endian, we swap 32-bit words.
			 */
			sa.sa_mask[0] = sa32.sa_mask[0] + ((long)(sa32.sa_mask[1]) << 32);
		}
	} else
#endif
	{
		r = umove(tcp, addr, &sa);
	}
	if (r < 0) {
		tprintf("{...}");
		goto after_sa;
	}
	/* Architectures using function pointers, like
	 * hppa, may need to manipulate the function pointer
	 * to compute the result of a comparison. However,
	 * the SA_HANDLER function pointer exists only in
	 * the address space of the traced process, and can't
	 * be manipulated by strace. In order to prevent the
	 * compiler from generating code to manipulate
	 * SA_HANDLER we cast the function pointers to long. */
	if ((long)sa.__sa_handler == (long)SIG_ERR)
		tprintf("{SIG_ERR, ");
	else if ((long)sa.__sa_handler == (long)SIG_DFL)
		tprintf("{SIG_DFL, ");
	else if ((long)sa.__sa_handler == (long)SIG_IGN)
		tprintf("{SIG_IGN, ");
	else
		tprintf("{%#lx, ", (long) sa.__sa_handler);
	/* Questionable code below.
	 * Kernel won't handle sys_rt_sigaction
	 * with wrong sigset size (just returns EINVAL)
	 * therefore tcp->u_arg[3(4)] _must_ be NSIG / 8 here,
	 * and we always use smaller memcpy. */
	sigemptyset(&sigset);
#ifdef LINUXSPARC
	if (tcp->u_arg[4] <= sizeof(sigset))
		memcpy(&sigset, &sa.sa_mask, tcp->u_arg[4]);
#else
	if (tcp->u_arg[3] <= sizeof(sigset))
		memcpy(&sigset, &sa.sa_mask, tcp->u_arg[3]);
#endif
	else
		memcpy(&sigset, &sa.sa_mask, sizeof(sigset));
	printsigmask(&sigset, 1);
	tprintf(", ");
	printflags(sigact_flags, sa.sa_flags, "SA_???");
#ifdef SA_RESTORER
	if (sa.sa_flags & SA_RESTORER)
		tprintf(", %p", sa.sa_restorer);
#endif
	tprintf("}");

 after_sa:
	if (entering(tcp))
		tprintf(", ");
	else
#ifdef LINUXSPARC
		tprintf(", %#lx, %lu", tcp->u_arg[3], tcp->u_arg[4]);
#elif defined(ALPHA)
		tprintf(", %lu, %#lx", tcp->u_arg[3], tcp->u_arg[4]);
#else
		tprintf(", %lu", tcp->u_arg[3]);
#endif
	return 0;
}

int
sys_rt_sigpending(struct tcb *tcp)
{
	sigset_t sigset;

	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else if (copy_sigset_len(tcp, tcp->u_arg[0],
					 &sigset, tcp->u_arg[1]) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 1);
	}
	return 0;
}

int
sys_rt_sigsuspend(struct tcb *tcp)
{
	if (entering(tcp)) {
		sigset_t sigm;
		if (copy_sigset_len(tcp, tcp->u_arg[0], &sigm, tcp->u_arg[1]) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigm, 1);
	}
	return 0;
}

int
sys_rt_sigqueueinfo(struct tcb *tcp)
{
	if (entering(tcp)) {
		siginfo_t si;
		tprintf("%lu, ", tcp->u_arg[0]);
		printsignal(tcp->u_arg[1]);
		tprintf(", ");
		if (umove(tcp, tcp->u_arg[2], &si) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			printsiginfo(&si, verbose(tcp));
	}
	return 0;
}

int sys_rt_sigtimedwait(struct tcb *tcp)
{
	if (entering(tcp)) {
		sigset_t sigset;

		if (copy_sigset_len(tcp, tcp->u_arg[0],
				    &sigset, tcp->u_arg[3]) < 0)
			tprintf("[?]");
		else
			printsigmask(&sigset, 1);
		tprintf(", ");
		/* This is the only "return" parameter, */
		if (tcp->u_arg[1] != 0)
			return 0;
		/* ... if it's NULL, can decode all on entry */
		tprintf("NULL, ");
	}
	else if (tcp->u_arg[1] != 0) {
		/* syscall exit, and u_arg[1] wasn't NULL */
		if (syserror(tcp))
			tprintf("%#lx, ", tcp->u_arg[1]);
		else {
			siginfo_t si;
			if (umove(tcp, tcp->u_arg[1], &si) < 0)
				tprintf("%#lx, ", tcp->u_arg[1]);
			else {
				printsiginfo(&si, verbose(tcp));
				tprintf(", ");
			}
		}
	}
	else {
		/* syscall exit, and u_arg[1] was NULL */
		return 0;
	}
	print_timespec(tcp, tcp->u_arg[2]);
	tprintf(", %d", (int) tcp->u_arg[3]);
	return 0;
};

int
sys_restart_syscall(struct tcb *tcp)
{
	if (entering(tcp))
		tprintf("<... resuming interrupted call ...>");
	return 0;
}

static int
do_signalfd(struct tcb *tcp, int flags_arg)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		print_sigset(tcp, tcp->u_arg[1], 1);
		tprintf(", %lu", tcp->u_arg[2]);
		if (flags_arg >= 0) {
			tprintf(", ");
			printflags(open_mode_flags, tcp->u_arg[flags_arg], "O_???");
		}
	}
	return 0;
}

int
sys_signalfd(struct tcb *tcp)
{
	return do_signalfd(tcp, -1);
}

int
sys_signalfd4(struct tcb *tcp)
{
	return do_signalfd(tcp, 3);
}
#endif /* LINUX */
