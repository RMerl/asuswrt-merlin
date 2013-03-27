/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *                     Linux for s390 port by D.J. Barrow
 *                    <barrow_dj@mail.yahoo.com,djbarrow@de.ibm.com>
 * Copyright (c) 2000 PocketPenguins Inc.  Linux for Hitachi SuperH
 *                    port by Greg Banks <gbanks@pocketpenguins.com>

 *
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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <signal.h>
#ifdef SUNOS4
#include <machine/reg.h>
#endif /* SUNOS4 */

#ifdef FREEBSD
#include <sys/ptrace.h>
#endif

#ifdef HAVE_SYS_REG_H
# include <sys/reg.h>
#ifndef PTRACE_PEEKUSR
# define PTRACE_PEEKUSR PTRACE_PEEKUSER
#endif
#ifndef PTRACE_POKEUSR
# define PTRACE_POKEUSR PTRACE_POKEUSER
#endif
#endif

#ifdef HAVE_LINUX_PTRACE_H
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

#if defined (LINUX) && defined (SPARC64)
# define r_pc r_tpc
# undef PTRACE_GETREGS
# define PTRACE_GETREGS PTRACE_GETREGS64
# undef PTRACE_SETREGS
# define PTRACE_SETREGS PTRACE_SETREGS64
#endif /* LINUX && SPARC64 */

#ifdef HAVE_LINUX_FUTEX_H
# include <linux/futex.h>
#endif
#ifdef LINUX
# ifndef FUTEX_WAIT
#  define FUTEX_WAIT 0
# endif
# ifndef FUTEX_WAKE
#  define FUTEX_WAKE 1
# endif
# ifndef FUTEX_FD
#  define FUTEX_FD 2
# endif
# ifndef FUTEX_REQUEUE
#  define FUTEX_REQUEUE 3
# endif
#endif /* LINUX */

#ifdef LINUX
#include <sched.h>
#include <asm/posix_types.h>
#undef GETGROUPS_T
#define GETGROUPS_T __kernel_gid_t
#undef GETGROUPS32_T
#define GETGROUPS32_T __kernel_gid32_t
#endif /* LINUX */

#if defined(LINUX) && defined(IA64)
# include <asm/ptrace_offsets.h>
# include <asm/rse.h>
#endif

#ifdef HAVE_PRCTL
# include <sys/prctl.h>

static const struct xlat prctl_options[] = {
#ifdef PR_MAXPROCS
	{ PR_MAXPROCS,		"PR_MAXPROCS"		},
#endif
#ifdef PR_ISBLOCKED
	{ PR_ISBLOCKED,		"PR_ISBLOCKED"		},
#endif
#ifdef PR_SETSTACKSIZE
	{ PR_SETSTACKSIZE,	"PR_SETSTACKSIZE"	},
#endif
#ifdef PR_GETSTACKSIZE
	{ PR_GETSTACKSIZE,	"PR_GETSTACKSIZE"	},
#endif
#ifdef PR_MAXPPROCS
	{ PR_MAXPPROCS,		"PR_MAXPPROCS"		},
#endif
#ifdef PR_UNBLKONEXEC
	{ PR_UNBLKONEXEC,	"PR_UNBLKONEXEC"	},
#endif
#ifdef PR_ATOMICSIM
	{ PR_ATOMICSIM,		"PR_ATOMICSIM"		},
#endif
#ifdef PR_SETEXITSIG
	{ PR_SETEXITSIG,	"PR_SETEXITSIG"		},
#endif
#ifdef PR_RESIDENT
	{ PR_RESIDENT,		"PR_RESIDENT"		},
#endif
#ifdef PR_ATTACHADDR
	{ PR_ATTACHADDR,	"PR_ATTACHADDR"		},
#endif
#ifdef PR_DETACHADDR
	{ PR_DETACHADDR,	"PR_DETACHADDR"		},
#endif
#ifdef PR_TERMCHILD
	{ PR_TERMCHILD,		"PR_TERMCHILD"		},
#endif
#ifdef PR_GETSHMASK
	{ PR_GETSHMASK,		"PR_GETSHMASK"		},
#endif
#ifdef PR_GETNSHARE
	{ PR_GETNSHARE,		"PR_GETNSHARE"		},
#endif
#ifdef PR_COREPID
	{ PR_COREPID,		"PR_COREPID"		},
#endif
#ifdef PR_ATTACHADDRPERM
	{ PR_ATTACHADDRPERM,	"PR_ATTACHADDRPERM"	},
#endif
#ifdef PR_PTHREADEXIT
	{ PR_PTHREADEXIT,	"PR_PTHREADEXIT"	},
#endif
#ifdef PR_SET_PDEATHSIG
	{ PR_SET_PDEATHSIG,	"PR_SET_PDEATHSIG"	},
#endif
#ifdef PR_GET_PDEATHSIG
	{ PR_GET_PDEATHSIG,	"PR_GET_PDEATHSIG"	},
#endif
#ifdef PR_GET_DUMPABLE
	{ PR_GET_DUMPABLE,	"PR_GET_DUMPABLE"	},
#endif
#ifdef PR_SET_DUMPABLE
	{ PR_SET_DUMPABLE,	"PR_SET_DUMPABLE"	},
#endif
#ifdef PR_GET_UNALIGN
	{ PR_GET_UNALIGN,	"PR_GET_UNALIGN"	},
#endif
#ifdef PR_SET_UNALIGN
	{ PR_SET_UNALIGN,	"PR_SET_UNALIGN"	},
#endif
#ifdef PR_GET_KEEPCAPS
	{ PR_GET_KEEPCAPS,	"PR_GET_KEEPCAPS"	},
#endif
#ifdef PR_SET_KEEPCAPS
	{ PR_SET_KEEPCAPS,	"PR_SET_KEEPCAPS"	},
#endif
#ifdef PR_GET_FPEMU
	{ PR_GET_FPEMU,		"PR_GET_FPEMU"		},
#endif
#ifdef PR_SET_FPEMU
	{ PR_SET_FPEMU,		"PR_SET_FPEMU"		},
#endif
#ifdef PR_GET_FPEXC
	{ PR_GET_FPEXC,		"PR_GET_FPEXC"		},
#endif
#ifdef PR_SET_FPEXC
	{ PR_SET_FPEXC,		"PR_SET_FPEXC"		},
#endif
#ifdef PR_GET_TIMING
	{ PR_GET_TIMING,	"PR_GET_TIMING"		},
#endif
#ifdef PR_SET_TIMING
	{ PR_SET_TIMING,	"PR_SET_TIMING"		},
#endif
#ifdef PR_SET_NAME
	{ PR_SET_NAME,		"PR_SET_NAME"		},
#endif
#ifdef PR_GET_NAME
	{ PR_GET_NAME,		"PR_GET_NAME"		},
#endif
#ifdef PR_GET_ENDIAN
	{ PR_GET_ENDIAN,	"PR_GET_ENDIAN"		},
#endif
#ifdef PR_SET_ENDIAN
	{ PR_SET_ENDIAN,	"PR_SET_ENDIAN"		},
#endif
#ifdef PR_GET_SECCOMP
	{ PR_GET_SECCOMP,	"PR_GET_SECCOMP"	},
#endif
#ifdef PR_SET_SECCOMP
	{ PR_SET_SECCOMP,	"PR_SET_SECCOMP"	},
#endif
#ifdef PR_GET_TSC
	{ PR_GET_TSC,		"PR_GET_TSC"		},
#endif
#ifdef PR_SET_TSC
	{ PR_SET_TSC,		"PR_SET_TSC"		},
#endif
#ifdef PR_GET_SECUREBITS
	{ PR_GET_SECUREBITS,	"PR_GET_SECUREBITS"	},
#endif
#ifdef PR_SET_SECUREBITS
	{ PR_SET_SECUREBITS,	"PR_SET_SECUREBITS"	},
#endif
	{ 0,			NULL			},
};


static const char *
unalignctl_string (unsigned int ctl)
{
	static char buf[16];

	switch (ctl) {
#ifdef PR_UNALIGN_NOPRINT
	      case PR_UNALIGN_NOPRINT:
		return "NOPRINT";
#endif
#ifdef PR_UNALIGN_SIGBUS
	      case PR_UNALIGN_SIGBUS:
		return "SIGBUS";
#endif
	      default:
		break;
	}
	sprintf(buf, "%x", ctl);
	return buf;
}


int
sys_prctl(tcp)
struct tcb *tcp;
{
	int i;

	if (entering(tcp)) {
		printxval(prctl_options, tcp->u_arg[0], "PR_???");
		switch (tcp->u_arg[0]) {
#ifdef PR_GETNSHARE
		case PR_GETNSHARE:
			break;
#endif
#ifdef PR_SET_PDEATHSIG
		case PR_SET_PDEATHSIG:
			tprintf(", %lu", tcp->u_arg[1]);
			break;
#endif
#ifdef PR_GET_PDEATHSIG
		case PR_GET_PDEATHSIG:
			break;
#endif
#ifdef PR_SET_DUMPABLE
		case PR_SET_DUMPABLE:
			tprintf(", %lu", tcp->u_arg[1]);
			break;
#endif
#ifdef PR_GET_DUMPABLE
		case PR_GET_DUMPABLE:
			break;
#endif
#ifdef PR_SET_UNALIGN
		case PR_SET_UNALIGN:
			tprintf(", %s", unalignctl_string(tcp->u_arg[1]));
			break;
#endif
#ifdef PR_GET_UNALIGN
		case PR_GET_UNALIGN:
			tprintf(", %#lx", tcp->u_arg[1]);
			break;
#endif
#ifdef PR_SET_KEEPCAPS
		case PR_SET_KEEPCAPS:
			tprintf(", %lu", tcp->u_arg[1]);
			break;
#endif
#ifdef PR_GET_KEEPCAPS
		case PR_GET_KEEPCAPS:
			break;
#endif
		default:
			for (i = 1; i < tcp->u_nargs; i++)
				tprintf(", %#lx", tcp->u_arg[i]);
			break;
		}
	} else {
		switch (tcp->u_arg[0]) {
#ifdef PR_GET_PDEATHSIG
		case PR_GET_PDEATHSIG:
			if (umove(tcp, tcp->u_arg[1], &i) < 0)
				tprintf(", %#lx", tcp->u_arg[1]);
			else
				tprintf(", {%u}", i);
			break;
#endif
#ifdef PR_GET_DUMPABLE
		case PR_GET_DUMPABLE:
			return RVAL_UDECIMAL;
#endif
#ifdef PR_GET_UNALIGN
		case PR_GET_UNALIGN:
			if (syserror(tcp) || umove(tcp, tcp->u_arg[1], &i) < 0)
				break;
			tcp->auxstr = unalignctl_string(i);
			return RVAL_STR;
#endif
#ifdef PR_GET_KEEPCAPS
		case PR_GET_KEEPCAPS:
			return RVAL_UDECIMAL;
#endif
		default:
			break;
		}
	}
	return 0;
}
#endif /* HAVE_PRCTL */

#if defined(FREEBSD) || defined(SUNOS4) || defined(SVR4)
int
sys_gethostid(tcp)
struct tcb *tcp;
{
	if (exiting(tcp))
		return RVAL_HEX;
	return 0;
}
#endif /* FREEBSD || SUNOS4 || SVR4 */

int
sys_sethostname(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printpathn(tcp, tcp->u_arg[0], tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}

#if defined(ALPHA) || defined(FREEBSD) || defined(SUNOS4) || defined(SVR4)
int
sys_gethostname(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else
			printpath(tcp, tcp->u_arg[0]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif /* ALPHA || FREEBSD || SUNOS4 || SVR4 */

int
sys_setdomainname(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printpathn(tcp, tcp->u_arg[0], tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}

#if !defined(LINUX)

int
sys_getdomainname(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else
			printpath(tcp, tcp->u_arg[0]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif /* !LINUX */

int
sys_exit(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		fprintf(stderr, "_exit returned!\n");
		return -1;
	}
	/* special case: we stop tracing this process, finish line now */
	tprintf("%ld) ", tcp->u_arg[0]);
	tabto(acolumn);
	tprintf("= ?");
	printtrailer();
	return 0;
}

int
internal_exit(struct tcb *tcp)
{
	if (entering(tcp)) {
		tcp->flags |= TCB_EXITING;
#ifdef __NR_exit_group
		if (known_scno(tcp) == __NR_exit_group)
			tcp->flags |= TCB_GROUP_EXITING;
#endif
	}
	return 0;
}

/* TCP is creating a child we want to follow.
   If there will be space in tcbtab for it, set TCB_FOLLOWFORK and return 0.
   If not, clear TCB_FOLLOWFORK, print an error, and return 1.  */
static void
fork_tcb(struct tcb *tcp)
{
	if (nprocs == tcbtabsize)
		expand_tcbtab();

	tcp->flags |= TCB_FOLLOWFORK;
}

#ifdef USE_PROCFS

int
sys_fork(struct tcb *tcp)
{
	if (exiting(tcp) && !syserror(tcp)) {
		if (getrval2(tcp)) {
			tcp->auxstr = "child process";
			return RVAL_UDECIMAL | RVAL_STR;
		}
	}
	return 0;
}

#if UNIXWARE > 2

int
sys_rfork(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf ("%ld", tcp->u_arg[0]);
	}
	else if (!syserror(tcp)) {
		if (getrval2(tcp)) {
			tcp->auxstr = "child process";
			return RVAL_UDECIMAL | RVAL_STR;
		}
	}
	return 0;
}

#endif

int
internal_fork(tcp)
struct tcb *tcp;
{
	struct tcb *tcpchild;

	if (exiting(tcp)) {
#ifdef SYS_rfork
		if (known_scno(tcp) == SYS_rfork && !(tcp->u_arg[0]&RFPROC))
			return 0;
#endif
		if (getrval2(tcp))
			return 0;
		if (!followfork)
			return 0;
		fork_tcb(tcp);
		if (syserror(tcp))
			return 0;
		tcpchild = alloctcb(tcp->u_rval);
		if (proc_open(tcpchild, 2) < 0)
			droptcb(tcpchild);
	}
	return 0;
}

#else /* !USE_PROCFS */

#ifdef LINUX

/* defines copied from linux/sched.h since we can't include that
 * ourselves (it conflicts with *lots* of libc includes)
 */
#define CSIGNAL         0x000000ff      /* signal mask to be sent at exit */
#define CLONE_VM        0x00000100      /* set if VM shared between processes */
#define CLONE_FS        0x00000200      /* set if fs info shared between processes */
#define CLONE_FILES     0x00000400      /* set if open files shared between processes */
#define CLONE_SIGHAND   0x00000800      /* set if signal handlers shared */
#define CLONE_IDLETASK  0x00001000      /* kernel-only flag */
#define CLONE_PTRACE    0x00002000      /* set if we want to let tracing continue on the child too */
#define CLONE_VFORK     0x00004000      /* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT    0x00008000      /* set if we want to have the same parent as the cloner */
#define CLONE_THREAD	0x00010000	/* Same thread group? */
#define CLONE_NEWNS	0x00020000	/* New namespace group? */
#define CLONE_SYSVSEM	0x00040000	/* share system V SEM_UNDO semantics */
#define CLONE_SETTLS	0x00080000	/* create a new TLS for the child */
#define CLONE_PARENT_SETTID	0x00100000	/* set the TID in the parent */
#define CLONE_CHILD_CLEARTID	0x00200000	/* clear the TID in the child */
#define CLONE_UNTRACED		0x00800000	/* set if the tracing process can't force CLONE_PTRACE on this clone */
#define CLONE_CHILD_SETTID	0x01000000	/* set the TID in the child */
#define CLONE_STOPPED		0x02000000	/* Start in stopped state */
#define CLONE_NEWUTS		0x04000000	/* New utsname group? */
#define CLONE_NEWIPC		0x08000000	/* New ipcs */
#define CLONE_NEWUSER		0x10000000	/* New user namespace */
#define CLONE_NEWPID		0x20000000	/* New pid namespace */
#define CLONE_NEWNET		0x40000000	/* New network namespace */
#define CLONE_IO		0x80000000	/* Clone io context */

static const struct xlat clone_flags[] = {
    { CLONE_VM,		"CLONE_VM"	},
    { CLONE_FS,		"CLONE_FS"	},
    { CLONE_FILES,	"CLONE_FILES"	},
    { CLONE_SIGHAND,	"CLONE_SIGHAND"	},
    { CLONE_IDLETASK,	"CLONE_IDLETASK"},
    { CLONE_PTRACE,	"CLONE_PTRACE"	},
    { CLONE_VFORK,	"CLONE_VFORK"	},
    { CLONE_PARENT,	"CLONE_PARENT"	},
    { CLONE_THREAD,	"CLONE_THREAD" },
    { CLONE_NEWNS,	"CLONE_NEWNS" },
    { CLONE_SYSVSEM,	"CLONE_SYSVSEM" },
    { CLONE_SETTLS,	"CLONE_SETTLS" },
    { CLONE_PARENT_SETTID,"CLONE_PARENT_SETTID" },
    { CLONE_CHILD_CLEARTID,"CLONE_CHILD_CLEARTID" },
    { CLONE_UNTRACED,	"CLONE_UNTRACED" },
    { CLONE_CHILD_SETTID,"CLONE_CHILD_SETTID" },
    { CLONE_STOPPED,	"CLONE_STOPPED" },
    { CLONE_NEWUTS,	"CLONE_NEWUTS" },
    { CLONE_NEWIPC,	"CLONE_NEWIPC" },
    { CLONE_NEWUSER,	"CLONE_NEWUSER" },
    { CLONE_NEWPID,	"CLONE_NEWPID" },
    { CLONE_NEWNET,	"CLONE_NEWNET" },
    { CLONE_IO,		"CLONE_IO" },
    { 0,		NULL		},
};

# ifdef I386
#  include <asm/ldt.h>
#   ifdef HAVE_STRUCT_USER_DESC
#    define modify_ldt_ldt_s user_desc
#   endif
extern void print_ldt_entry();
# endif

# if defined IA64
#  define ARG_FLAGS	0
#  define ARG_STACK	1
#  define ARG_STACKSIZE	(known_scno(tcp) == SYS_clone2 ? 2 : -1)
#  define ARG_PTID	(known_scno(tcp) == SYS_clone2 ? 3 : 2)
#  define ARG_CTID	(known_scno(tcp) == SYS_clone2 ? 4 : 3)
#  define ARG_TLS	(known_scno(tcp) == SYS_clone2 ? 5 : 4)
# elif defined S390 || defined S390X || defined CRISV10 || defined CRISV32
#  define ARG_STACK	0
#  define ARG_FLAGS	1
#  define ARG_PTID	2
#  define ARG_CTID	3
#  define ARG_TLS	4
# elif defined X86_64 || defined ALPHA
#  define ARG_FLAGS	0
#  define ARG_STACK	1
#  define ARG_PTID	2
#  define ARG_CTID	3
#  define ARG_TLS	4
# else
#  define ARG_FLAGS	0
#  define ARG_STACK	1
#  define ARG_PTID	2
#  define ARG_TLS	3
#  define ARG_CTID	4
# endif

int
sys_clone(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		const char *sep = "|";
		unsigned long flags = tcp->u_arg[ARG_FLAGS];
		tprintf("child_stack=%#lx, ", tcp->u_arg[ARG_STACK]);
# ifdef ARG_STACKSIZE
		if (ARG_STACKSIZE != -1)
			tprintf("stack_size=%#lx, ",
				tcp->u_arg[ARG_STACKSIZE]);
# endif
		tprintf("flags=");
		if (!printflags(clone_flags, flags &~ CSIGNAL, NULL))
			sep = "";
		if ((flags & CSIGNAL) != 0)
			tprintf("%s%s", sep, signame(flags & CSIGNAL));
		if ((flags & (CLONE_PARENT_SETTID|CLONE_CHILD_SETTID
			      |CLONE_CHILD_CLEARTID|CLONE_SETTLS)) == 0)
			return 0;
		if (flags & CLONE_PARENT_SETTID)
			tprintf(", parent_tidptr=%#lx", tcp->u_arg[ARG_PTID]);
		if (flags & CLONE_SETTLS) {
# ifdef I386
			struct modify_ldt_ldt_s copy;
			if (umove(tcp, tcp->u_arg[ARG_TLS], &copy) != -1) {
				tprintf(", {entry_number:%d, ",
					copy.entry_number);
				if (!verbose(tcp))
					tprintf("...}");
				else
					print_ldt_entry(&copy);
			}
			else
# endif
				tprintf(", tls=%#lx", tcp->u_arg[ARG_TLS]);
		}
		if (flags & (CLONE_CHILD_SETTID|CLONE_CHILD_CLEARTID))
			tprintf(", child_tidptr=%#lx", tcp->u_arg[ARG_CTID]);
	}
	return 0;
}

int
sys_unshare(struct tcb *tcp)
{
	if (entering(tcp))
		printflags(clone_flags, tcp->u_arg[0], "CLONE_???");
	return 0;
}
#endif /* LINUX */

int
sys_fork(tcp)
struct tcb *tcp;
{
	if (exiting(tcp))
		return RVAL_UDECIMAL;
	return 0;
}

int
change_syscall(struct tcb *tcp, int new)
{
#ifdef LINUX
#if defined(I386)
	/* Attempt to make vfork into fork, which we can follow. */
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(ORIG_EAX * 4), new) < 0)
		return -1;
	return 0;
#elif defined(X86_64)
	/* Attempt to make vfork into fork, which we can follow. */
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(ORIG_RAX * 8), new) < 0)
		return -1;
	return 0;
#elif defined(POWERPC)
	if (ptrace(PTRACE_POKEUSER, tcp->pid,
		   (char*)(sizeof(unsigned long)*PT_R0), new) < 0)
		return -1;
	return 0;
#elif defined(S390) || defined(S390X)
	/* s390 linux after 2.4.7 has a hook in entry.S to allow this */
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(PT_GPR2), new)<0)
		return -1;
	return 0;
#elif defined(M68K)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(4*PT_ORIG_D0), new)<0)
		return -1;
	return 0;
#elif defined(SPARC) || defined(SPARC64)
	struct pt_regs regs;
	if (ptrace(PTRACE_GETREGS, tcp->pid, (char*)&regs, 0)<0)
		return -1;
	regs.u_regs[U_REG_G1] = new;
	if (ptrace(PTRACE_SETREGS, tcp->pid, (char*)&regs, 0)<0)
		return -1;
	return 0;
#elif defined(MIPS)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(REG_V0), new)<0)
		return -1;
	return 0;
#elif defined(ALPHA)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(REG_A3), new)<0)
		return -1;
	return 0;
#elif defined(AVR32)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(REG_R8), new) < 0)
		return -1;
	return 0;
#elif defined(BFIN)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(REG_P0), new)<0)
		return -1;
	return 0;
#elif defined(IA64)
	if (ia32) {
		switch (new) {
		case 2:
			break;	/* x86 SYS_fork */
		case SYS_clone:
			new = 120;
			break;
		default:
			fprintf(stderr, "%s: unexpected syscall %d\n",
				__FUNCTION__, new);
			return -1;
		}
		if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(PT_R1), new)<0)
			return -1;
	} else if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(PT_R15), new)<0)
		return -1;
	return 0;
#elif defined(HPPA)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(PT_GR20), new)<0)
		return -1;
	return 0;
#elif defined(SH)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(4*(REG_REG0+3)), new)<0)
		return -1;
	return 0;
#elif defined(SH64)
	/* Top half of reg encodes the no. of args n as 0x1n.
	   Assume 0 args as kernel never actually checks... */
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(REG_SYSCALL),
				0x100000 | new) < 0)
		return -1;
	return 0;
#elif defined(CRISV10) || defined(CRISV32)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(4*PT_R9), new) < 0)
		return -1;
	return 0;
#elif defined(ARM)
	/* Some kernels support this, some (pre-2.6.16 or so) don't.  */
# ifndef PTRACE_SET_SYSCALL
#  define PTRACE_SET_SYSCALL 23
# endif

	if (ptrace (PTRACE_SET_SYSCALL, tcp->pid, 0, new & 0xffff) != 0)
		return -1;

	return 0;
#elif defined(TILE)
	if (ptrace(PTRACE_POKEUSER, tcp->pid,
		   (char*)PTREGS_OFFSET_REG(0),
		   new) != 0)
		return -1;
	return 0;
#elif defined(MICROBLAZE)
	if (ptrace(PTRACE_POKEUSER, tcp->pid, (char*)(PT_GPR(0)), new)<0)
		return -1;
	return 0;
#else
#warning Do not know how to handle change_syscall for this architecture
#endif /* architecture */
#endif /* LINUX */
	return -1;
}

#ifdef LINUX
int
handle_new_child(struct tcb *tcp, int pid, int bpt)
{
	struct tcb *tcpchild;

#ifdef CLONE_PTRACE		/* See new setbpt code.  */
	tcpchild = pid2tcb(pid);
	if (tcpchild != NULL) {
		/* The child already reported its startup trap
		   before the parent reported its syscall return.  */
		if ((tcpchild->flags
		     & (TCB_STARTUP|TCB_ATTACHED|TCB_SUSPENDED))
		    != (TCB_STARTUP|TCB_ATTACHED|TCB_SUSPENDED))
			fprintf(stderr, "\
[preattached child %d of %d in weird state!]\n",
				pid, tcp->pid);
	}
	else
#endif /* CLONE_PTRACE */
	{
		fork_tcb(tcp);
		tcpchild = alloctcb(pid);
	}

#ifndef CLONE_PTRACE
	/* Attach to the new child */
	if (ptrace(PTRACE_ATTACH, pid, (char *) 1, 0) < 0) {
		if (bpt)
			clearbpt(tcp);
		perror("PTRACE_ATTACH");
		fprintf(stderr, "Too late?\n");
		droptcb(tcpchild);
		return 0;
	}
#endif /* !CLONE_PTRACE */

	if (bpt)
		clearbpt(tcp);

	tcpchild->flags |= TCB_ATTACHED;
	/* Child has BPT too, must be removed on first occasion.  */
	if (bpt) {
		tcpchild->flags |= TCB_BPTSET;
		tcpchild->baddr = tcp->baddr;
		memcpy(tcpchild->inst, tcp->inst,
			sizeof tcpchild->inst);
	}
	tcpchild->parent = tcp;
	tcp->nchildren++;
	if (tcpchild->flags & TCB_SUSPENDED) {
		/* The child was born suspended, due to our having
		   forced CLONE_PTRACE.  */
		if (bpt)
			clearbpt(tcpchild);

		tcpchild->flags &= ~(TCB_SUSPENDED|TCB_STARTUP);
		if (ptrace_restart(PTRACE_SYSCALL, tcpchild, 0) < 0)
			return -1;

		if (!qflag)
			fprintf(stderr, "\
Process %u resumed (parent %d ready)\n",
				pid, tcp->pid);
	}
	else {
		if (!qflag)
			fprintf(stderr, "Process %d attached\n", pid);
	}

#ifdef TCB_CLONE_THREAD
	if (sysent[tcp->scno].sys_func == sys_clone)
	{
		/*
		 * Save the flags used in this call,
		 * in case we point TCP to our parent below.
		 */
		int call_flags = tcp->u_arg[ARG_FLAGS];
		if ((tcp->flags & TCB_CLONE_THREAD) &&
		    tcp->parent != NULL) {
			/* The parent in this clone is itself a
			   thread belonging to another process.
			   There is no meaning to the parentage
			   relationship of the new child with the
			   thread, only with the process.  We
			   associate the new thread with our
			   parent.  Since this is done for every
			   new thread, there will never be a
			   TCB_CLONE_THREAD process that has
			   children.  */
			--tcp->nchildren;
			tcp = tcp->parent;
			tcpchild->parent = tcp;
			++tcp->nchildren;
		}
		if (call_flags & CLONE_THREAD) {
			tcpchild->flags |= TCB_CLONE_THREAD;
			++tcp->nclone_threads;
		}
		if ((call_flags & CLONE_PARENT) &&
		    !(call_flags & CLONE_THREAD)) {
			--tcp->nchildren;
			tcpchild->parent = NULL;
			if (tcp->parent != NULL) {
				tcp = tcp->parent;
				tcpchild->parent = tcp;
				++tcp->nchildren;
			}
		}
	}
#endif /* TCB_CLONE_THREAD */
	return 0;
}

int
internal_fork(struct tcb *tcp)
{
	if ((ptrace_setoptions
	    & (PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK))
	   == (PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK))
		return 0;

	if (entering(tcp)) {
		tcp->flags &= ~TCB_FOLLOWFORK;
		if (!followfork)
			return 0;
		/*
		 * In occasion of using PTRACE_O_TRACECLONE, we won't see the
		 * new child if clone is called with flag CLONE_UNTRACED, so
		 * we keep the same logic with that option and don't trace it.
		 */
		if ((sysent[tcp->scno].sys_func == sys_clone) &&
		    (tcp->u_arg[ARG_FLAGS] & CLONE_UNTRACED))
			return 0;
		fork_tcb(tcp);
		if (setbpt(tcp) < 0)
			return 0;
	} else {
		int pid;
		int bpt;

		if (!(tcp->flags & TCB_FOLLOWFORK))
			return 0;

		bpt = tcp->flags & TCB_BPTSET;

		if (syserror(tcp)) {
			if (bpt)
				clearbpt(tcp);
			return 0;
		}

		pid = tcp->u_rval;

		return handle_new_child(tcp, pid, bpt);
	}
	return 0;
}

#else /* !LINUX */

int
internal_fork(tcp)
struct tcb *tcp;
{
	struct tcb *tcpchild;
	int pid;
	int dont_follow = 0;

#ifdef SYS_vfork
	if (known_scno(tcp) == SYS_vfork) {
		/* Attempt to make vfork into fork, which we can follow. */
		if (change_syscall(tcp, SYS_fork) < 0)
			dont_follow = 1;
	}
#endif
	if (entering(tcp)) {
		if (!followfork || dont_follow)
			return 0;
		fork_tcb(tcp);
		if (setbpt(tcp) < 0)
			return 0;
	}
	else {
		int bpt = tcp->flags & TCB_BPTSET;

		if (!(tcp->flags & TCB_FOLLOWFORK))
			return 0;
		if (bpt)
			clearbpt(tcp);

		if (syserror(tcp))
			return 0;

		pid = tcp->u_rval;
		fork_tcb(tcp);
		tcpchild = alloctcb(pid);
#ifdef SUNOS4
#ifdef oldway
		/* The child must have run before it can be attached. */
		{
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
			select(0, NULL, NULL, NULL, &tv);
		}
		if (ptrace(PTRACE_ATTACH, pid, (char *)1, 0) < 0) {
			perror("PTRACE_ATTACH");
			fprintf(stderr, "Too late?\n");
			droptcb(tcpchild);
			return 0;
		}
#else /* !oldway */
		/* Try to catch the new process as soon as possible. */
		{
			int i;
			for (i = 0; i < 1024; i++)
				if (ptrace(PTRACE_ATTACH, pid, (char *) 1, 0) >= 0)
					break;
			if (i == 1024) {
				perror("PTRACE_ATTACH");
				fprintf(stderr, "Too late?\n");
				droptcb(tcpchild);
				return 0;
			}
		}
#endif /* !oldway */
#endif /* SUNOS4 */
		tcpchild->flags |= TCB_ATTACHED;
		/* Child has BPT too, must be removed on first occasion */
		if (bpt) {
			tcpchild->flags |= TCB_BPTSET;
			tcpchild->baddr = tcp->baddr;
			memcpy(tcpchild->inst, tcp->inst,
				sizeof tcpchild->inst);
		}
		tcpchild->parent = tcp;
		tcp->nchildren++;
		if (!qflag)
			fprintf(stderr, "Process %d attached\n", pid);
	}
	return 0;
}

#endif /* !LINUX */

#endif /* !USE_PROCFS */

#if defined(SUNOS4) || defined(LINUX) || defined(FREEBSD)

int
sys_vfork(tcp)
struct tcb *tcp;
{
	if (exiting(tcp))
		return RVAL_UDECIMAL;
	return 0;
}

#endif /* SUNOS4 || LINUX || FREEBSD */

#ifndef LINUX

static char idstr[16];

int
sys_getpid(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		sprintf(idstr, "ppid %lu", getrval2(tcp));
		tcp->auxstr = idstr;
		return RVAL_STR;
	}
	return 0;
}

int
sys_getuid(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		sprintf(idstr, "euid %lu", getrval2(tcp));
		tcp->auxstr = idstr;
		return RVAL_STR;
	}
	return 0;
}

int
sys_getgid(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		sprintf(idstr, "egid %lu", getrval2(tcp));
		tcp->auxstr = idstr;
		return RVAL_STR;
	}
	return 0;
}

#endif /* !LINUX */

#ifdef LINUX

int sys_getuid(struct tcb *tcp)
{
	if (exiting(tcp))
		tcp->u_rval = (uid_t) tcp->u_rval;
	return RVAL_UDECIMAL;
}

int sys_setfsuid(struct tcb *tcp)
{
	if (entering(tcp))
		tprintf("%u", (uid_t) tcp->u_arg[0]);
	else
		tcp->u_rval = (uid_t) tcp->u_rval;
	return RVAL_UDECIMAL;
}

int
sys_setuid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%u", (uid_t) tcp->u_arg[0]);
	}
	return 0;
}

int
sys_setgid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%u", (gid_t) tcp->u_arg[0]);
	}
	return 0;
}

int
sys_getresuid(struct tcb *tcp)
{
	if (exiting(tcp)) {
		__kernel_uid_t uid;
		if (syserror(tcp))
			tprintf("%#lx, %#lx, %#lx", tcp->u_arg[0],
				tcp->u_arg[1], tcp->u_arg[2]);
		else {
			if (umove(tcp, tcp->u_arg[0], &uid) < 0)
				tprintf("%#lx, ", tcp->u_arg[0]);
			else
				tprintf("[%lu], ", (unsigned long) uid);
			if (umove(tcp, tcp->u_arg[1], &uid) < 0)
				tprintf("%#lx, ", tcp->u_arg[1]);
			else
				tprintf("[%lu], ", (unsigned long) uid);
			if (umove(tcp, tcp->u_arg[2], &uid) < 0)
				tprintf("%#lx", tcp->u_arg[2]);
			else
				tprintf("[%lu]", (unsigned long) uid);
		}
	}
	return 0;
}

int
sys_getresgid(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		__kernel_gid_t gid;
		if (syserror(tcp))
			tprintf("%#lx, %#lx, %#lx", tcp->u_arg[0],
				tcp->u_arg[1], tcp->u_arg[2]);
		else {
			if (umove(tcp, tcp->u_arg[0], &gid) < 0)
				tprintf("%#lx, ", tcp->u_arg[0]);
			else
				tprintf("[%lu], ", (unsigned long) gid);
			if (umove(tcp, tcp->u_arg[1], &gid) < 0)
				tprintf("%#lx, ", tcp->u_arg[1]);
			else
				tprintf("[%lu], ", (unsigned long) gid);
			if (umove(tcp, tcp->u_arg[2], &gid) < 0)
				tprintf("%#lx", tcp->u_arg[2]);
			else
				tprintf("[%lu]", (unsigned long) gid);
		}
	}
	return 0;
}

#endif /* LINUX */

int
sys_setreuid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printuid("", tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
	}
	return 0;
}

int
sys_setregid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printuid("", tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
	}
	return 0;
}

#if defined(LINUX) || defined(FREEBSD)
int
sys_setresuid(tcp)
     struct tcb *tcp;
{
	if (entering(tcp)) {
		printuid("", tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
		printuid(", ", tcp->u_arg[2]);
	}
	return 0;
}
int
sys_setresgid(tcp)
     struct tcb *tcp;
{
	if (entering(tcp)) {
		printuid("", tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
		printuid(", ", tcp->u_arg[2]);
	}
	return 0;
}

#endif /* LINUX || FREEBSD */

int
sys_setgroups(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		unsigned long len, size, start, cur, end, abbrev_end;
		GETGROUPS_T gid;
		int failed = 0;

		len = tcp->u_arg[0];
		tprintf("%lu, ", len);
		if (len == 0) {
			tprintf("[]");
			return 0;
		}
		start = tcp->u_arg[1];
		if (start == 0) {
			tprintf("NULL");
			return 0;
		}
		size = len * sizeof(gid);
		end = start + size;
		if (!verbose(tcp) || size / sizeof(gid) != len || end < start) {
			tprintf("%#lx", start);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(gid);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf("[");
		for (cur = start; cur < end; cur += sizeof(gid)) {
			if (cur > start)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof(gid), (char *) &gid) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			tprintf("%lu", (unsigned long) gid);
		}
		tprintf("]");
		if (failed)
			tprintf(" %#lx", tcp->u_arg[1]);
	}
	return 0;
}

int
sys_getgroups(tcp)
struct tcb *tcp;
{
	unsigned long len;

	if (entering(tcp)) {
		len = tcp->u_arg[0];
		tprintf("%lu, ", len);
	} else {
		unsigned long size, start, cur, end, abbrev_end;
		GETGROUPS_T gid;
		int failed = 0;

		len = tcp->u_rval;
		if (len == 0) {
			tprintf("[]");
			return 0;
		}
		start = tcp->u_arg[1];
		if (start == 0) {
			tprintf("NULL");
			return 0;
		}
		if (tcp->u_arg[0] == 0) {
			tprintf("%#lx", start);
			return 0;
		}
		size = len * sizeof(gid);
		end = start + size;
		if (!verbose(tcp) || tcp->u_arg[0] == 0 ||
		    size / sizeof(gid) != len || end < start) {
			tprintf("%#lx", start);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(gid);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf("[");
		for (cur = start; cur < end; cur += sizeof(gid)) {
			if (cur > start)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof(gid), (char *) &gid) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			tprintf("%lu", (unsigned long) gid);
		}
		tprintf("]");
		if (failed)
			tprintf(" %#lx", tcp->u_arg[1]);
	}
	return 0;
}

#ifdef LINUX
int
sys_setgroups32(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		unsigned long len, size, start, cur, end, abbrev_end;
		GETGROUPS32_T gid;
		int failed = 0;

		len = tcp->u_arg[0];
		tprintf("%lu, ", len);
		if (len == 0) {
			tprintf("[]");
			return 0;
		}
		start = tcp->u_arg[1];
		if (start == 0) {
			tprintf("NULL");
			return 0;
		}
		size = len * sizeof(gid);
		end = start + size;
		if (!verbose(tcp) || size / sizeof(gid) != len || end < start) {
			tprintf("%#lx", start);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(gid);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf("[");
		for (cur = start; cur < end; cur += sizeof(gid)) {
			if (cur > start)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof(gid), (char *) &gid) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			tprintf("%lu", (unsigned long) gid);
		}
		tprintf("]");
		if (failed)
			tprintf(" %#lx", tcp->u_arg[1]);
	}
	return 0;
}

int
sys_getgroups32(tcp)
struct tcb *tcp;
{
	unsigned long len;

	if (entering(tcp)) {
		len = tcp->u_arg[0];
		tprintf("%lu, ", len);
	} else {
		unsigned long size, start, cur, end, abbrev_end;
		GETGROUPS32_T gid;
		int failed = 0;

		len = tcp->u_rval;
		if (len == 0) {
			tprintf("[]");
			return 0;
		}
		start = tcp->u_arg[1];
		if (start == 0) {
			tprintf("NULL");
			return 0;
		}
		size = len * sizeof(gid);
		end = start + size;
		if (!verbose(tcp) || tcp->u_arg[0] == 0 ||
		    size / sizeof(gid) != len || end < start) {
			tprintf("%#lx", start);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = start + max_strlen * sizeof(gid);
			if (abbrev_end < start)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf("[");
		for (cur = start; cur < end; cur += sizeof(gid)) {
			if (cur > start)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof(gid), (char *) &gid) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			tprintf("%lu", (unsigned long) gid);
		}
		tprintf("]");
		if (failed)
			tprintf(" %#lx", tcp->u_arg[1]);
	}
	return 0;
}
#endif /* LINUX */

#if defined(ALPHA) || defined(SUNOS4) || defined(SVR4)
int
sys_setpgrp(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
#ifndef SVR4
		tprintf("%lu, %lu", tcp->u_arg[0], tcp->u_arg[1]);
#endif /* !SVR4 */
	}
	return 0;
}
#endif /* ALPHA || SUNOS4 || SVR4 */

int
sys_getpgrp(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
#ifndef SVR4
		tprintf("%lu", tcp->u_arg[0]);
#endif /* !SVR4 */
	}
	return 0;
}

int
sys_getsid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu", tcp->u_arg[0]);
	}
	return 0;
}

int
sys_setsid(tcp)
struct tcb *tcp;
{
	return 0;
}

int
sys_getpgid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu", tcp->u_arg[0]);
	}
	return 0;
}

int
sys_setpgid(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu, %lu", tcp->u_arg[0], tcp->u_arg[1]);
	}
	return 0;
}

#if UNIXWARE >= 2

#include <sys/privilege.h>


static const struct xlat procpriv_cmds [] = {
	{ SETPRV,	"SETPRV"	},
	{ CLRPRV,	"CLRPRV"	},
	{ PUTPRV,	"PUTPRV"	},
	{ GETPRV,	"GETPRV"	},
	{ CNTPRV,	"CNTPRV"	},
	{ 0,		NULL		},
};


static const struct xlat procpriv_priv [] = {
	{ P_OWNER,	"P_OWNER"	},
	{ P_AUDIT,	"P_AUDIT"	},
	{ P_COMPAT,	"P_COMPAT"	},
	{ P_DACREAD,	"P_DACREAD"	},
	{ P_DACWRITE,	"P_DACWRITE"	},
	{ P_DEV,	"P_DEV"		},
	{ P_FILESYS,	"P_FILESYS"	},
	{ P_MACREAD,	"P_MACREAD"	},
	{ P_MACWRITE,	"P_MACWRITE"	},
	{ P_MOUNT,	"P_MOUNT"	},
	{ P_MULTIDIR,	"P_MULTIDIR"	},
	{ P_SETPLEVEL,	"P_SETPLEVEL"	},
	{ P_SETSPRIV,	"P_SETSPRIV"	},
	{ P_SETUID,	"P_SETUID"	},
	{ P_SYSOPS,	"P_SYSOPS"	},
	{ P_SETUPRIV,	"P_SETUPRIV"	},
	{ P_DRIVER,	"P_DRIVER"	},
	{ P_RTIME,	"P_RTIME"	},
	{ P_MACUPGRADE,	"P_MACUPGRADE"	},
	{ P_FSYSRANGE,	"P_FSYSRANGE"	},
	{ P_SETFLEVEL,	"P_SETFLEVEL"	},
	{ P_AUDITWR,	"P_AUDITWR"	},
	{ P_TSHAR,	"P_TSHAR"	},
	{ P_PLOCK,	"P_PLOCK"	},
	{ P_CORE,	"P_CORE"	},
	{ P_LOADMOD,	"P_LOADMOD"	},
	{ P_BIND,	"P_BIND"	},
	{ P_ALLPRIVS,	"P_ALLPRIVS"	},
	{ 0,		NULL		},
};


static const struct xlat procpriv_type [] = {
	{ PS_FIX,	"PS_FIX"	},
	{ PS_INH,	"PS_INH"	},
	{ PS_MAX,	"PS_MAX"	},
	{ PS_WKG,	"PS_WKG"	},
	{ 0,		NULL		},
};


static void
printpriv(struct tcb *tcp, long addr, int len, const struct xlat *opt)
{
	priv_t buf [128];
	int max = verbose (tcp) ? sizeof buf / sizeof buf [0] : 10;
	int dots = len > max;
	int i;

	if (len > max) len = max;

	if (len <= 0 ||
	    umoven (tcp, addr, len * sizeof buf[0], (char *) buf) < 0)
	{
		tprintf ("%#lx", addr);
		return;
	}

	tprintf ("[");

	for (i = 0; i < len; ++i) {
		const char *t, *p;

		if (i) tprintf (", ");

		if ((t = xlookup (procpriv_type, buf [i] & PS_TYPE)) &&
		    (p = xlookup (procpriv_priv, buf [i] & ~PS_TYPE)))
		{
			tprintf ("%s|%s", t, p);
		}
		else {
			tprintf ("%#lx", buf [i]);
		}
	}

	if (dots) tprintf (" ...");

	tprintf ("]");
}


int
sys_procpriv(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printxval(procpriv_cmds, tcp->u_arg[0], "???PRV");
		switch (tcp->u_arg[0]) {
		    case CNTPRV:
			tprintf(", %#lx, %ld", tcp->u_arg[1], tcp->u_arg[2]);
			break;

		    case GETPRV:
			break;

		    default:
			tprintf (", ");
			printpriv (tcp, tcp->u_arg[1], tcp->u_arg[2]);
			tprintf (", %ld", tcp->u_arg[2]);
		}
	}
	else if (tcp->u_arg[0] == GETPRV) {
		if (syserror (tcp)) {
			tprintf(", %#lx, %ld", tcp->u_arg[1], tcp->u_arg[2]);
		}
		else {
			tprintf (", ");
			printpriv (tcp, tcp->u_arg[1], tcp->u_rval);
			tprintf (", %ld", tcp->u_arg[2]);
		}
	}

	return 0;
}

#endif /* UNIXWARE */


static void
printargv(struct tcb *tcp, long addr)
{
	union {
		unsigned int p32;
		unsigned long p64;
		char data[sizeof(long)];
	} cp;
	const char *sep;
	int n = 0;

	cp.p64 = 1;
	for (sep = ""; !abbrev(tcp) || n < max_strlen / 2; sep = ", ", ++n) {
		if (umoven(tcp, addr, personality_wordsize[current_personality],
			   cp.data) < 0) {
			tprintf("%#lx", addr);
			return;
		}
		if (personality_wordsize[current_personality] == 4)
			cp.p64 = cp.p32;
		if (cp.p64 == 0)
			break;
		tprintf("%s", sep);
		printstr(tcp, cp.p64, -1);
		addr += personality_wordsize[current_personality];
	}
	if (cp.p64)
		tprintf("%s...", sep);
}

static void
printargc(const char *fmt, struct tcb *tcp, long addr)
{
	int count;
	char *cp;

	for (count = 0; umove(tcp, addr, &cp) >= 0 && cp != NULL; count++) {
		addr += sizeof(char *);
	}
	tprintf(fmt, count, count == 1 ? "" : "s");
}

#if defined(SPARC) || defined(SPARC64) || defined(SUNOS4)
int
sys_execv(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		if (!verbose(tcp))
			tprintf(", %#lx", tcp->u_arg[1]);
		else {
			tprintf(", [");
			printargv(tcp, tcp->u_arg[1]);
			tprintf("]");
		}
	}
	return 0;
}
#endif /* SPARC || SPARC64 || SUNOS4 */

int
sys_execve(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		if (!verbose(tcp))
			tprintf(", %#lx", tcp->u_arg[1]);
		else {
			tprintf(", [");
			printargv(tcp, tcp->u_arg[1]);
			tprintf("]");
		}
		if (!verbose(tcp))
			tprintf(", %#lx", tcp->u_arg[2]);
		else if (abbrev(tcp))
			printargc(", [/* %d var%s */]", tcp, tcp->u_arg[2]);
		else {
			tprintf(", [");
			printargv(tcp, tcp->u_arg[2]);
			tprintf("]");
		}
	}
	return 0;
}

#if UNIXWARE > 2

int sys_rexecve(tcp)
struct tcb *tcp;
{
	if (entering (tcp)) {
		sys_execve (tcp);
		tprintf (", %ld", tcp->u_arg[3]);
	}
	return 0;
}

#endif

int
internal_exec(tcp)
struct tcb *tcp;
{
#ifdef SUNOS4
	if (exiting(tcp) && !syserror(tcp) && followfork)
		fixvfork(tcp);
#endif /* SUNOS4 */
#if defined LINUX && defined TCB_WAITEXECVE
	if (exiting(tcp) && syserror(tcp))
		tcp->flags &= ~TCB_WAITEXECVE;
	else
		tcp->flags |= TCB_WAITEXECVE;
#endif /* LINUX && TCB_WAITEXECVE */
	return 0;
}

#ifdef LINUX
#ifndef __WNOTHREAD
#define __WNOTHREAD	0x20000000
#endif
#ifndef __WALL
#define __WALL		0x40000000
#endif
#ifndef __WCLONE
#define __WCLONE	0x80000000
#endif
#endif /* LINUX */

static const struct xlat wait4_options[] = {
	{ WNOHANG,	"WNOHANG"	},
#ifndef WSTOPPED
	{ WUNTRACED,	"WUNTRACED"	},
#endif
#ifdef WEXITED
	{ WEXITED,	"WEXITED"	},
#endif
#ifdef WTRAPPED
	{ WTRAPPED,	"WTRAPPED"	},
#endif
#ifdef WSTOPPED
	{ WSTOPPED,	"WSTOPPED"	},
#endif
#ifdef WCONTINUED
	{ WCONTINUED,	"WCONTINUED"	},
#endif
#ifdef WNOWAIT
	{ WNOWAIT,	"WNOWAIT"	},
#endif
#ifdef __WCLONE
	{ __WCLONE,	"__WCLONE"	},
#endif
#ifdef __WALL
	{ __WALL,	"__WALL"	},
#endif
#ifdef __WNOTHREAD
	{ __WNOTHREAD,	"__WNOTHREAD"	},
#endif
	{ 0,		NULL		},
};

#if !defined WCOREFLAG && defined WCOREFLG
# define WCOREFLAG WCOREFLG
#endif
#ifndef WCOREFLAG
# define WCOREFLAG 0x80
#endif
#ifndef WCOREDUMP
# define WCOREDUMP(status) ((status) & 0200)
#endif


#ifndef W_STOPCODE
#define W_STOPCODE(sig)		((sig) << 8 | 0x7f)
#endif
#ifndef W_EXITCODE
#define W_EXITCODE(ret, sig)	((ret) << 8 | (sig))
#endif

static int
printstatus(status)
int status;
{
	int exited = 0;

	/*
	 * Here is a tricky presentation problem.  This solution
	 * is still not entirely satisfactory but since there
	 * are no wait status constructors it will have to do.
	 */
	if (WIFSTOPPED(status)) {
		tprintf("[{WIFSTOPPED(s) && WSTOPSIG(s) == %s}",
			signame(WSTOPSIG(status)));
		status &= ~W_STOPCODE(WSTOPSIG(status));
	}
	else if (WIFSIGNALED(status)) {
		tprintf("[{WIFSIGNALED(s) && WTERMSIG(s) == %s%s}",
			signame(WTERMSIG(status)),
			WCOREDUMP(status) ? " && WCOREDUMP(s)" : "");
		status &= ~(W_EXITCODE(0, WTERMSIG(status)) | WCOREFLAG);
	}
	else if (WIFEXITED(status)) {
		tprintf("[{WIFEXITED(s) && WEXITSTATUS(s) == %d}",
			WEXITSTATUS(status));
		exited = 1;
		status &= ~W_EXITCODE(WEXITSTATUS(status), 0);
	}
	else {
		tprintf("[%#x]", status);
		return 0;
	}

	if (status == 0)
		tprintf("]");
	else
		tprintf(" | %#x]", status);

	return exited;
}

static int
printwaitn(struct tcb *tcp, int n, int bitness)
{
	int status;
#ifdef SUNOS4
	int exited = 0;
#endif

	if (entering(tcp)) {
#ifdef LINUX
		/* On Linux, kernel-side pid_t is typedef'ed to int
		 * on all arches. Also, glibc-2.8 truncates wait3 and wait4
		 * pid argument to int on 64bit arches, producing,
		 * for example, wait4(4294967295, ...) instead of -1
		 * in strace. We have to use int here, not long.
		 */
		int pid = tcp->u_arg[0];
		tprintf("%d, ", pid);
#else
		/*
		 * Sign-extend a 32-bit value when that's what it is.
		 */
		long pid = tcp->u_arg[0];
		if (personality_wordsize[current_personality] < sizeof pid)
			pid = (long) (int) pid;
		tprintf("%ld, ", pid);
#endif
	} else {
		/* status */
		if (!tcp->u_arg[1])
			tprintf("NULL");
		else if (syserror(tcp) || tcp->u_rval == 0)
			tprintf("%#lx", tcp->u_arg[1]);
		else if (umove(tcp, tcp->u_arg[1], &status) < 0)
			tprintf("[?]");
		else
#ifdef SUNOS4
			exited =
#endif
			printstatus(status);
		/* options */
		tprintf(", ");
		printflags(wait4_options, tcp->u_arg[2], "W???");
		if (n == 4) {
			tprintf(", ");
			/* usage */
			if (!tcp->u_arg[3])
				tprintf("NULL");
#ifdef LINUX
			else if (tcp->u_rval > 0) {
#ifdef ALPHA
				if (bitness)
					printrusage32(tcp, tcp->u_arg[3]);
				else
#endif
					printrusage(tcp, tcp->u_arg[3]);
			}
#endif /* LINUX */
#ifdef SUNOS4
			else if (tcp->u_rval > 0 && exited)
				printrusage(tcp, tcp->u_arg[3]);
#endif /* SUNOS4 */
			else
				tprintf("%#lx", tcp->u_arg[3]);
		}
	}
	return 0;
}

int
internal_wait(tcp, flagarg)
struct tcb *tcp;
int flagarg;
{
	int got_kids;

#ifdef TCB_CLONE_THREAD
	if (tcp->flags & TCB_CLONE_THREAD)
		/* The children we wait for are our parent's children.  */
		got_kids = (tcp->parent->nchildren
			    > tcp->parent->nclone_threads);
	else
		got_kids = (tcp->nchildren > tcp->nclone_threads);
#else
	got_kids = tcp->nchildren > 0;
#endif

	if (entering(tcp) && got_kids) {
		/* There are children that this parent should block for.
		   But ptrace made us the parent of the traced children
		   and the real parent will get ECHILD from the wait call.

		   XXX If we attached with strace -f -p PID, then there
		   may be untraced dead children the parent could be reaping
		   now, but we make him block.  */

		/* ??? WTA: fix bug with hanging children */

		if (!(tcp->u_arg[flagarg] & WNOHANG)) {
			/*
			 * There are traced children.  We'll make the parent
			 * block to avoid a false ECHILD error due to our
			 * ptrace having stolen the children.  However,
			 * we shouldn't block if there are zombies to reap.
			 * XXX doesn't handle pgrp matches (u_arg[0]==0,<-1)
			 */
			struct tcb *child = NULL;
			if (tcp->nzombies > 0 &&
			    (tcp->u_arg[0] == -1 ||
			     (child = pid2tcb(tcp->u_arg[0])) == NULL))
				return 0;
			if (tcp->u_arg[0] > 0) {
				/*
				 * If the parent waits for a specified child
				 * PID, then it must get ECHILD right away
				 * if that PID is not one of its children.
				 * Make sure that the requested PID matches
				 * one of the parent's children that we are
				 * tracing, and don't suspend it otherwise.
				 */
				if (child == NULL)
					child = pid2tcb(tcp->u_arg[0]);
				if (child == NULL || child->parent != (
#ifdef TCB_CLONE_THREAD
					    (tcp->flags & TCB_CLONE_THREAD)
					    ? tcp->parent :
#endif
					    tcp) ||
				    (child->flags & TCB_EXITING))
					return 0;
			}
			tcp->flags |= TCB_SUSPENDED;
			tcp->waitpid = tcp->u_arg[0];
#ifdef TCB_CLONE_THREAD
			if (tcp->flags & TCB_CLONE_THREAD)
				tcp->parent->nclone_waiting++;
#endif
		}
	}
	if (exiting(tcp) && tcp->u_error == ECHILD && got_kids) {
		if (tcp->u_arg[flagarg] & WNOHANG) {
			/* We must force a fake result of 0 instead of
			   the ECHILD error.  */
			return force_result(tcp, 0, 0);
		}
	}
	else if (exiting(tcp) && tcp->u_error == 0 && tcp->u_rval > 0 &&
		 tcp->nzombies > 0 && pid2tcb(tcp->u_rval) == NULL) {
		/*
		 * We just reaped a child we don't know about,
		 * presumably a zombie we already droptcb'd.
		 */
		tcp->nzombies--;
	}
	return 0;
}

#ifdef SVR4

int
sys_wait(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		/* The library wrapper stuffs this into the user variable. */
		if (!syserror(tcp))
			printstatus(getrval2(tcp));
	}
	return 0;
}

#endif /* SVR4 */

#ifdef FREEBSD
int
sys_wait(tcp)
struct tcb *tcp;
{
	int status;

	if (exiting(tcp)) {
		if (!syserror(tcp)) {
			if (umove(tcp, tcp->u_arg[0], &status) < 0)
				tprintf("%#lx", tcp->u_arg[0]);
			else
				printstatus(status);
		}
	}
	return 0;
}
#endif

int
sys_waitpid(tcp)
struct tcb *tcp;
{
	return printwaitn(tcp, 3, 0);
}

int
sys_wait4(tcp)
struct tcb *tcp;
{
	return printwaitn(tcp, 4, 0);
}

#ifdef ALPHA
int
sys_osf_wait4(tcp)
struct tcb *tcp;
{
	return printwaitn(tcp, 4, 1);
}
#endif

#if defined SVR4 || defined LINUX

static const struct xlat waitid_types[] = {
	{ P_PID,	"P_PID"		},
#ifdef P_PPID
	{ P_PPID,	"P_PPID"	},
#endif
	{ P_PGID,	"P_PGID"	},
#ifdef P_SID
	{ P_SID,	"P_SID"		},
#endif
#ifdef P_CID
	{ P_CID,	"P_CID"		},
#endif
#ifdef P_UID
	{ P_UID,	"P_UID"		},
#endif
#ifdef P_GID
	{ P_GID,	"P_GID"		},
#endif
	{ P_ALL,	"P_ALL"		},
#ifdef P_LWPID
	{ P_LWPID,	"P_LWPID"	},
#endif
	{ 0,		NULL		},
};

int
sys_waitid(struct tcb *tcp)
{
	siginfo_t si;

	if (entering(tcp)) {
		printxval(waitid_types, tcp->u_arg[0], "P_???");
		tprintf(", %ld, ", tcp->u_arg[1]);
	}
	else {
		/* siginfo */
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[2]);
		else if (umove(tcp, tcp->u_arg[2], &si) < 0)
			tprintf("{???}");
		else
			printsiginfo(&si, verbose(tcp));
		/* options */
		tprintf(", ");
		printflags(wait4_options, tcp->u_arg[3], "W???");
		if (tcp->u_nargs > 4) {
			/* usage */
			tprintf(", ");
			if (!tcp->u_arg[4])
				tprintf("NULL");
			else if (tcp->u_error)
				tprintf("%#lx", tcp->u_arg[4]);
			else
				printrusage(tcp, tcp->u_arg[4]);
		}
	}
	return 0;
}

#endif /* SVR4 or LINUX */

int
sys_alarm(tcp)
struct tcb *tcp;
{
	if (entering(tcp))
		tprintf("%lu", tcp->u_arg[0]);
	return 0;
}

int
sys_uname(tcp)
struct tcb *tcp;
{
	struct utsname uname;

	if (exiting(tcp)) {
		if (syserror(tcp) || !verbose(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else if (umove(tcp, tcp->u_arg[0], &uname) < 0)
			tprintf("{...}");
		else if (!abbrev(tcp)) {

			tprintf("{sysname=\"%s\", nodename=\"%s\", ",
				uname.sysname, uname.nodename);
			tprintf("release=\"%s\", version=\"%s\", ",
				uname.release, uname.version);
			tprintf("machine=\"%s\"", uname.machine);
#ifdef LINUX
#ifndef __GLIBC__
			tprintf(", domainname=\"%s\"", uname.domainname);
#endif
#endif
			tprintf("}");
		}
		else
			tprintf("{sys=\"%s\", node=\"%s\", ...}",
				uname.sysname, uname.nodename);
	}
	return 0;
}

#ifndef SVR4

static const struct xlat ptrace_cmds[] = {
# ifndef FREEBSD
	{ PTRACE_TRACEME,	"PTRACE_TRACEME"	},
	{ PTRACE_PEEKTEXT,	"PTRACE_PEEKTEXT",	},
	{ PTRACE_PEEKDATA,	"PTRACE_PEEKDATA",	},
	{ PTRACE_PEEKUSER,	"PTRACE_PEEKUSER",	},
	{ PTRACE_POKETEXT,	"PTRACE_POKETEXT",	},
	{ PTRACE_POKEDATA,	"PTRACE_POKEDATA",	},
	{ PTRACE_POKEUSER,	"PTRACE_POKEUSER",	},
	{ PTRACE_CONT,		"PTRACE_CONT"		},
	{ PTRACE_KILL,		"PTRACE_KILL"		},
	{ PTRACE_SINGLESTEP,	"PTRACE_SINGLESTEP"	},
	{ PTRACE_ATTACH,	"PTRACE_ATTACH"		},
	{ PTRACE_DETACH,	"PTRACE_DETACH"		},
#  ifdef PTRACE_GETREGS
	{ PTRACE_GETREGS,	"PTRACE_GETREGS"	},
#  endif
#  ifdef PTRACE_SETREGS
	{ PTRACE_SETREGS,	"PTRACE_SETREGS"	},
#  endif
#  ifdef PTRACE_GETFPREGS
	{ PTRACE_GETFPREGS,	"PTRACE_GETFPREGS",	},
#  endif
#  ifdef PTRACE_SETFPREGS
	{ PTRACE_SETFPREGS,	"PTRACE_SETFPREGS",	},
#  endif
#  ifdef PTRACE_GETFPXREGS
	{ PTRACE_GETFPXREGS,	"PTRACE_GETFPXREGS",	},
#  endif
#  ifdef PTRACE_SETFPXREGS
	{ PTRACE_SETFPXREGS,	"PTRACE_SETFPXREGS",	},
#  endif
#  ifdef PTRACE_GETVRREGS
	{ PTRACE_GETVRREGS,	"PTRACE_GETVRREGS",	},
#  endif
#  ifdef PTRACE_SETVRREGS
	{ PTRACE_SETVRREGS,	"PTRACE_SETVRREGS",	},
#  endif
#  ifdef PTRACE_SETOPTIONS
	{ PTRACE_SETOPTIONS,	"PTRACE_SETOPTIONS",	},
#  endif
#  ifdef PTRACE_GETEVENTMSG
	{ PTRACE_GETEVENTMSG,	"PTRACE_GETEVENTMSG",	},
#  endif
#  ifdef PTRACE_GETSIGINFO
	{ PTRACE_GETSIGINFO,	"PTRACE_GETSIGINFO",	},
#  endif
#  ifdef PTRACE_SETSIGINFO
	{ PTRACE_SETSIGINFO,	"PTRACE_SETSIGINFO",	},
#  endif
#  ifdef PTRACE_GETREGSET
	{ PTRACE_GETREGSET,	"PTRACE_GETREGSET",	},
#  endif
#  ifdef PTRACE_SETREGSET
	{ PTRACE_SETREGSET,	"PTRACE_SETREGSET",	},
#  endif
#  ifdef PTRACE_SET_SYSCALL
	{ PTRACE_SET_SYSCALL,	"PTRACE_SET_SYSCALL",	},
#  endif
#  ifdef SUNOS4
	{ PTRACE_READDATA,	"PTRACE_READDATA"	},
	{ PTRACE_WRITEDATA,	"PTRACE_WRITEDATA"	},
	{ PTRACE_READTEXT,	"PTRACE_READTEXT"	},
	{ PTRACE_WRITETEXT,	"PTRACE_WRITETEXT"	},
	{ PTRACE_GETFPAREGS,	"PTRACE_GETFPAREGS"	},
	{ PTRACE_SETFPAREGS,	"PTRACE_SETFPAREGS"	},
#   ifdef SPARC
	{ PTRACE_GETWINDOW,	"PTRACE_GETWINDOW"	},
	{ PTRACE_SETWINDOW,	"PTRACE_SETWINDOW"	},
#   else /* !SPARC */
	{ PTRACE_22,		"PTRACE_22"		},
	{ PTRACE_23,		"PTRACE_3"		},
#   endif /* !SPARC */
#  endif /* SUNOS4 */
	{ PTRACE_SYSCALL,	"PTRACE_SYSCALL"	},
#  ifdef SUNOS4
	{ PTRACE_DUMPCORE,	"PTRACE_DUMPCORE"	},
#   ifdef I386
	{ PTRACE_SETWRBKPT,	"PTRACE_SETWRBKPT"	},
	{ PTRACE_SETACBKPT,	"PTRACE_SETACBKPT"	},
	{ PTRACE_CLRDR7,	"PTRACE_CLRDR7"		},
#   else /* !I386 */
	{ PTRACE_26,		"PTRACE_26"		},
	{ PTRACE_27,		"PTRACE_27"		},
	{ PTRACE_28,		"PTRACE_28"		},
#   endif /* !I386 */
	{ PTRACE_GETUCODE,	"PTRACE_GETUCODE"	},
#  endif /* SUNOS4 */

# else /* FREEBSD */

	{ PT_TRACE_ME,		"PT_TRACE_ME"		},
	{ PT_READ_I,		"PT_READ_I"		},
	{ PT_READ_D,		"PT_READ_D"		},
	{ PT_WRITE_I,		"PT_WRITE_I"		},
	{ PT_WRITE_D,		"PT_WRITE_D"		},
#  ifdef PT_READ_U
	{ PT_READ_U,		"PT_READ_U"		},
#  endif
	{ PT_CONTINUE,		"PT_CONTINUE"		},
	{ PT_KILL,		"PT_KILL"		},
	{ PT_STEP,		"PT_STEP"		},
	{ PT_ATTACH,		"PT_ATTACH"		},
	{ PT_DETACH,		"PT_DETACH"		},
	{ PT_GETREGS,		"PT_GETREGS"		},
	{ PT_SETREGS,		"PT_SETREGS"		},
	{ PT_GETFPREGS,		"PT_GETFPREGS"		},
	{ PT_SETFPREGS,		"PT_SETFPREGS"		},
	{ PT_GETDBREGS,		"PT_GETDBREGS"		},
	{ PT_SETDBREGS,		"PT_SETDBREGS"		},
# endif /* FREEBSD */
	{ 0,			NULL			},
};

# ifndef FREEBSD
#  ifdef PTRACE_SETOPTIONS
static const struct xlat ptrace_setoptions_flags[] = {
#   ifdef PTRACE_O_TRACESYSGOOD
	{ PTRACE_O_TRACESYSGOOD,"PTRACE_O_TRACESYSGOOD"	},
#   endif
#   ifdef PTRACE_O_TRACEFORK
	{ PTRACE_O_TRACEFORK,	"PTRACE_O_TRACEFORK"	},
#   endif
#   ifdef PTRACE_O_TRACEVFORK
	{ PTRACE_O_TRACEVFORK,	"PTRACE_O_TRACEVFORK"	},
#   endif
#   ifdef PTRACE_O_TRACECLONE
	{ PTRACE_O_TRACECLONE,	"PTRACE_O_TRACECLONE"	},
#   endif
#   ifdef PTRACE_O_TRACEEXEC
	{ PTRACE_O_TRACEEXEC,	"PTRACE_O_TRACEEXEC"	},
#   endif
#   ifdef PTRACE_O_TRACEVFORKDONE
	{ PTRACE_O_TRACEVFORKDONE,"PTRACE_O_TRACEVFORKDONE"},
#   endif
#   ifdef PTRACE_O_TRACEEXIT
	{ PTRACE_O_TRACEEXIT,	"PTRACE_O_TRACEEXIT"	},
#   endif
	{ 0,			NULL			},
};
#  endif /* PTRACE_SETOPTIONS */
# endif /* !FREEBSD */

# ifndef FREEBSD
const struct xlat struct_user_offsets[] = {
#  ifdef LINUX
#   if defined(S390) || defined(S390X)
	{ PT_PSWMASK,		"psw_mask"				},
	{ PT_PSWADDR,		"psw_addr"				},
	{ PT_GPR0,		"gpr0"					},
	{ PT_GPR1,		"gpr1"					},
	{ PT_GPR2,		"gpr2"					},
	{ PT_GPR3,		"gpr3"					},
	{ PT_GPR4,		"gpr4"					},
	{ PT_GPR5,		"gpr5"					},
	{ PT_GPR6,		"gpr6"					},
	{ PT_GPR7,		"gpr7"					},
	{ PT_GPR8,		"gpr8"					},
	{ PT_GPR9,		"gpr9"					},
	{ PT_GPR10,		"gpr10"					},
	{ PT_GPR11,		"gpr11"					},
	{ PT_GPR12,		"gpr12"					},
	{ PT_GPR13,		"gpr13"					},
	{ PT_GPR14,		"gpr14"					},
	{ PT_GPR15,		"gpr15"					},
	{ PT_ACR0,		"acr0"					},
	{ PT_ACR1,		"acr1"					},
	{ PT_ACR2,		"acr2"					},
	{ PT_ACR3,		"acr3"					},
	{ PT_ACR4,		"acr4"					},
	{ PT_ACR5,		"acr5"					},
	{ PT_ACR6,		"acr6"					},
	{ PT_ACR7,		"acr7"					},
	{ PT_ACR8,		"acr8"					},
	{ PT_ACR9,		"acr9"					},
	{ PT_ACR10,		"acr10"					},
	{ PT_ACR11,		"acr11"					},
	{ PT_ACR12,		"acr12"					},
	{ PT_ACR13,		"acr13"					},
	{ PT_ACR14,		"acr14"					},
	{ PT_ACR15,		"acr15"					},
	{ PT_ORIGGPR2,		"orig_gpr2"				},
	{ PT_FPC,		"fpc"					},
#    if defined(S390)
	{ PT_FPR0_HI,		"fpr0.hi"				},
	{ PT_FPR0_LO,		"fpr0.lo"				},
	{ PT_FPR1_HI,		"fpr1.hi"				},
	{ PT_FPR1_LO,		"fpr1.lo"				},
	{ PT_FPR2_HI,		"fpr2.hi"				},
	{ PT_FPR2_LO,		"fpr2.lo"				},
	{ PT_FPR3_HI,		"fpr3.hi"				},
	{ PT_FPR3_LO,		"fpr3.lo"				},
	{ PT_FPR4_HI,		"fpr4.hi"				},
	{ PT_FPR4_LO,		"fpr4.lo"				},
	{ PT_FPR5_HI,		"fpr5.hi"				},
	{ PT_FPR5_LO,		"fpr5.lo"				},
	{ PT_FPR6_HI,		"fpr6.hi"				},
	{ PT_FPR6_LO,		"fpr6.lo"				},
	{ PT_FPR7_HI,		"fpr7.hi"				},
	{ PT_FPR7_LO,		"fpr7.lo"				},
	{ PT_FPR8_HI,		"fpr8.hi"				},
	{ PT_FPR8_LO,		"fpr8.lo"				},
	{ PT_FPR9_HI,		"fpr9.hi"				},
	{ PT_FPR9_LO,		"fpr9.lo"				},
	{ PT_FPR10_HI,		"fpr10.hi"				},
	{ PT_FPR10_LO,		"fpr10.lo"				},
	{ PT_FPR11_HI,		"fpr11.hi"				},
	{ PT_FPR11_LO,		"fpr11.lo"				},
	{ PT_FPR12_HI,		"fpr12.hi"				},
	{ PT_FPR12_LO,		"fpr12.lo"				},
	{ PT_FPR13_HI,		"fpr13.hi"				},
	{ PT_FPR13_LO,		"fpr13.lo"				},
	{ PT_FPR14_HI,		"fpr14.hi"				},
	{ PT_FPR14_LO,		"fpr14.lo"				},
	{ PT_FPR15_HI,		"fpr15.hi"				},
	{ PT_FPR15_LO,		"fpr15.lo"				},
#    endif
#    if defined(S390X)
	{ PT_FPR0,		"fpr0"					},
	{ PT_FPR1,		"fpr1"					},
	{ PT_FPR2,		"fpr2"					},
	{ PT_FPR3,		"fpr3"					},
	{ PT_FPR4,		"fpr4"					},
	{ PT_FPR5,		"fpr5"					},
	{ PT_FPR6,		"fpr6"					},
	{ PT_FPR7,		"fpr7"					},
	{ PT_FPR8,		"fpr8"					},
	{ PT_FPR9,		"fpr9"					},
	{ PT_FPR10,		"fpr10"					},
	{ PT_FPR11,		"fpr11"					},
	{ PT_FPR12,		"fpr12"					},
	{ PT_FPR13,		"fpr13"					},
	{ PT_FPR14,		"fpr14"					},
	{ PT_FPR15,		"fpr15"					},
#    endif
	{ PT_CR_9,		"cr9"					},
	{ PT_CR_10,		"cr10"					},
	{ PT_CR_11,		"cr11"					},
	{ PT_IEEE_IP,           "ieee_exception_ip"                     },
#   elif defined(SPARC)
	/* XXX No support for these offsets yet. */
#   elif defined(HPPA)
	/* XXX No support for these offsets yet. */
#   elif defined(POWERPC)
#    ifndef PT_ORIG_R3
#     define PT_ORIG_R3 34
#    endif
#    define REGSIZE (sizeof(unsigned long))
	{ REGSIZE*PT_R0,		"r0"				},
	{ REGSIZE*PT_R1,		"r1"				},
	{ REGSIZE*PT_R2,		"r2"				},
	{ REGSIZE*PT_R3,		"r3"				},
	{ REGSIZE*PT_R4,		"r4"				},
	{ REGSIZE*PT_R5,		"r5"				},
	{ REGSIZE*PT_R6,		"r6"				},
	{ REGSIZE*PT_R7,		"r7"				},
	{ REGSIZE*PT_R8,		"r8"				},
	{ REGSIZE*PT_R9,		"r9"				},
	{ REGSIZE*PT_R10,		"r10"				},
	{ REGSIZE*PT_R11,		"r11"				},
	{ REGSIZE*PT_R12,		"r12"				},
	{ REGSIZE*PT_R13,		"r13"				},
	{ REGSIZE*PT_R14,		"r14"				},
	{ REGSIZE*PT_R15,		"r15"				},
	{ REGSIZE*PT_R16,		"r16"				},
	{ REGSIZE*PT_R17,		"r17"				},
	{ REGSIZE*PT_R18,		"r18"				},
	{ REGSIZE*PT_R19,		"r19"				},
	{ REGSIZE*PT_R20,		"r20"				},
	{ REGSIZE*PT_R21,		"r21"				},
	{ REGSIZE*PT_R22,		"r22"				},
	{ REGSIZE*PT_R23,		"r23"				},
	{ REGSIZE*PT_R24,		"r24"				},
	{ REGSIZE*PT_R25,		"r25"				},
	{ REGSIZE*PT_R26,		"r26"				},
	{ REGSIZE*PT_R27,		"r27"				},
	{ REGSIZE*PT_R28,		"r28"				},
	{ REGSIZE*PT_R29,		"r29"				},
	{ REGSIZE*PT_R30,		"r30"				},
	{ REGSIZE*PT_R31,		"r31"				},
	{ REGSIZE*PT_NIP,		"NIP"				},
	{ REGSIZE*PT_MSR,		"MSR"				},
	{ REGSIZE*PT_ORIG_R3,		"ORIG_R3"			},
	{ REGSIZE*PT_CTR,		"CTR"				},
	{ REGSIZE*PT_LNK,		"LNK"				},
	{ REGSIZE*PT_XER,		"XER"				},
	{ REGSIZE*PT_CCR,		"CCR"				},
	{ REGSIZE*PT_FPR0,		"FPR0"				},
#    undef REGSIZE
#   elif defined(ALPHA)
	{ 0,			"r0"					},
	{ 1,			"r1"					},
	{ 2,			"r2"					},
	{ 3,			"r3"					},
	{ 4,			"r4"					},
	{ 5,			"r5"					},
	{ 6,			"r6"					},
	{ 7,			"r7"					},
	{ 8,			"r8"					},
	{ 9,			"r9"					},
	{ 10,			"r10"					},
	{ 11,			"r11"					},
	{ 12,			"r12"					},
	{ 13,			"r13"					},
	{ 14,			"r14"					},
	{ 15,			"r15"					},
	{ 16,			"r16"					},
	{ 17,			"r17"					},
	{ 18,			"r18"					},
	{ 19,			"r19"					},
	{ 20,			"r20"					},
	{ 21,			"r21"					},
	{ 22,			"r22"					},
	{ 23,			"r23"					},
	{ 24,			"r24"					},
	{ 25,			"r25"					},
	{ 26,			"r26"					},
	{ 27,			"r27"					},
	{ 28,			"r28"					},
	{ 29,			"gp"					},
	{ 30,			"fp"					},
	{ 31,			"zero"					},
	{ 32,			"fp0"					},
	{ 33,			"fp"					},
	{ 34,			"fp2"					},
	{ 35,			"fp3"					},
	{ 36,			"fp4"					},
	{ 37,			"fp5"					},
	{ 38,			"fp6"					},
	{ 39,			"fp7"					},
	{ 40,			"fp8"					},
	{ 41,			"fp9"					},
	{ 42,			"fp10"					},
	{ 43,			"fp11"					},
	{ 44,			"fp12"					},
	{ 45,			"fp13"					},
	{ 46,			"fp14"					},
	{ 47,			"fp15"					},
	{ 48,			"fp16"					},
	{ 49,			"fp17"					},
	{ 50,			"fp18"					},
	{ 51,			"fp19"					},
	{ 52,			"fp20"					},
	{ 53,			"fp21"					},
	{ 54,			"fp22"					},
	{ 55,			"fp23"					},
	{ 56,			"fp24"					},
	{ 57,			"fp25"					},
	{ 58,			"fp26"					},
	{ 59,			"fp27"					},
	{ 60,			"fp28"					},
	{ 61,			"fp29"					},
	{ 62,			"fp30"					},
	{ 63,			"fp31"					},
	{ 64,			"pc"					},
#   elif defined(IA64)
	{ PT_F32, "f32" }, { PT_F33, "f33" }, { PT_F34, "f34" },
	{ PT_F35, "f35" }, { PT_F36, "f36" }, { PT_F37, "f37" },
	{ PT_F38, "f38" }, { PT_F39, "f39" }, { PT_F40, "f40" },
	{ PT_F41, "f41" }, { PT_F42, "f42" }, { PT_F43, "f43" },
	{ PT_F44, "f44" }, { PT_F45, "f45" }, { PT_F46, "f46" },
	{ PT_F47, "f47" }, { PT_F48, "f48" }, { PT_F49, "f49" },
	{ PT_F50, "f50" }, { PT_F51, "f51" }, { PT_F52, "f52" },
	{ PT_F53, "f53" }, { PT_F54, "f54" }, { PT_F55, "f55" },
	{ PT_F56, "f56" }, { PT_F57, "f57" }, { PT_F58, "f58" },
	{ PT_F59, "f59" }, { PT_F60, "f60" }, { PT_F61, "f61" },
	{ PT_F62, "f62" }, { PT_F63, "f63" }, { PT_F64, "f64" },
	{ PT_F65, "f65" }, { PT_F66, "f66" }, { PT_F67, "f67" },
	{ PT_F68, "f68" }, { PT_F69, "f69" }, { PT_F70, "f70" },
	{ PT_F71, "f71" }, { PT_F72, "f72" }, { PT_F73, "f73" },
	{ PT_F74, "f74" }, { PT_F75, "f75" }, { PT_F76, "f76" },
	{ PT_F77, "f77" }, { PT_F78, "f78" }, { PT_F79, "f79" },
	{ PT_F80, "f80" }, { PT_F81, "f81" }, { PT_F82, "f82" },
	{ PT_F83, "f83" }, { PT_F84, "f84" }, { PT_F85, "f85" },
	{ PT_F86, "f86" }, { PT_F87, "f87" }, { PT_F88, "f88" },
	{ PT_F89, "f89" }, { PT_F90, "f90" }, { PT_F91, "f91" },
	{ PT_F92, "f92" }, { PT_F93, "f93" }, { PT_F94, "f94" },
	{ PT_F95, "f95" }, { PT_F96, "f96" }, { PT_F97, "f97" },
	{ PT_F98, "f98" }, { PT_F99, "f99" }, { PT_F100, "f100" },
	{ PT_F101, "f101" }, { PT_F102, "f102" }, { PT_F103, "f103" },
	{ PT_F104, "f104" }, { PT_F105, "f105" }, { PT_F106, "f106" },
	{ PT_F107, "f107" }, { PT_F108, "f108" }, { PT_F109, "f109" },
	{ PT_F110, "f110" }, { PT_F111, "f111" }, { PT_F112, "f112" },
	{ PT_F113, "f113" }, { PT_F114, "f114" }, { PT_F115, "f115" },
	{ PT_F116, "f116" }, { PT_F117, "f117" }, { PT_F118, "f118" },
	{ PT_F119, "f119" }, { PT_F120, "f120" }, { PT_F121, "f121" },
	{ PT_F122, "f122" }, { PT_F123, "f123" }, { PT_F124, "f124" },
	{ PT_F125, "f125" }, { PT_F126, "f126" }, { PT_F127, "f127" },
	/* switch stack: */
	{ PT_F2, "f2" }, { PT_F3, "f3" }, { PT_F4, "f4" },
	{ PT_F5, "f5" }, { PT_F10, "f10" }, { PT_F11, "f11" },
	{ PT_F12, "f12" }, { PT_F13, "f13" }, { PT_F14, "f14" },
	{ PT_F15, "f15" }, { PT_F16, "f16" }, { PT_F17, "f17" },
	{ PT_F18, "f18" }, { PT_F19, "f19" }, { PT_F20, "f20" },
	{ PT_F21, "f21" }, { PT_F22, "f22" }, { PT_F23, "f23" },
	{ PT_F24, "f24" }, { PT_F25, "f25" }, { PT_F26, "f26" },
	{ PT_F27, "f27" }, { PT_F28, "f28" }, { PT_F29, "f29" },
	{ PT_F30, "f30" }, { PT_F31, "f31" }, { PT_R4, "r4" },
	{ PT_R5, "r5" }, { PT_R6, "r6" }, { PT_R7, "r7" },
	{ PT_B1, "b1" }, { PT_B2, "b2" }, { PT_B3, "b3" },
	{ PT_B4, "b4" }, { PT_B5, "b5" },
	{ PT_AR_EC, "ar.ec" }, { PT_AR_LC, "ar.lc" },
	/* pt_regs */
	{ PT_CR_IPSR, "psr" }, { PT_CR_IIP, "ip" },
	{ PT_CFM, "cfm" }, { PT_AR_UNAT, "ar.unat" },
	{ PT_AR_PFS, "ar.pfs" }, { PT_AR_RSC, "ar.rsc" },
	{ PT_AR_RNAT, "ar.rnat" }, { PT_AR_BSPSTORE, "ar.bspstore" },
	{ PT_PR, "pr" }, { PT_B6, "b6" }, { PT_AR_BSP, "ar.bsp" },
	{ PT_R1, "r1" }, { PT_R2, "r2" }, { PT_R3, "r3" },
	{ PT_R12, "r12" }, { PT_R13, "r13" }, { PT_R14, "r14" },
	{ PT_R15, "r15" }, { PT_R8, "r8" }, { PT_R9, "r9" },
	{ PT_R10, "r10" }, { PT_R11, "r11" }, { PT_R16, "r16" },
	{ PT_R17, "r17" }, { PT_R18, "r18" }, { PT_R19, "r19" },
	{ PT_R20, "r20" }, { PT_R21, "r21" }, { PT_R22, "r22" },
	{ PT_R23, "r23" }, { PT_R24, "r24" }, { PT_R25, "r25" },
	{ PT_R26, "r26" }, { PT_R27, "r27" }, { PT_R28, "r28" },
	{ PT_R29, "r29" }, { PT_R30, "r30" }, { PT_R31, "r31" },
	{ PT_AR_CCV, "ar.ccv" }, { PT_AR_FPSR, "ar.fpsr" },
	{ PT_B0, "b0" }, { PT_B7, "b7" }, { PT_F6, "f6" },
	{ PT_F7, "f7" }, { PT_F8, "f8" }, { PT_F9, "f9" },
#    ifdef PT_AR_CSD
	{ PT_AR_CSD, "ar.csd" },
#    endif
#    ifdef PT_AR_SSD
	{ PT_AR_SSD, "ar.ssd" },
#    endif
	{ PT_DBR, "dbr" }, { PT_IBR, "ibr" }, { PT_PMD, "pmd" },
#   elif defined(I386)
	{ 4*EBX,		"4*EBX"					},
	{ 4*ECX,		"4*ECX"					},
	{ 4*EDX,		"4*EDX"					},
	{ 4*ESI,		"4*ESI"					},
	{ 4*EDI,		"4*EDI"					},
	{ 4*EBP,		"4*EBP"					},
	{ 4*EAX,		"4*EAX"					},
	{ 4*DS,			"4*DS"					},
	{ 4*ES,			"4*ES"					},
	{ 4*FS,			"4*FS"					},
	{ 4*GS,			"4*GS"					},
	{ 4*ORIG_EAX,		"4*ORIG_EAX"				},
	{ 4*EIP,		"4*EIP"					},
	{ 4*CS,			"4*CS"					},
	{ 4*EFL,		"4*EFL"					},
	{ 4*UESP,		"4*UESP"				},
	{ 4*SS,			"4*SS"					},
#   elif defined(X86_64)
	{ 8*R15, 		"8*R15"					},
	{ 8*R14, 		"8*R14"					},
	{ 8*R13, 		"8*R13"					},
	{ 8*R12, 		"8*R12"					},
	{ 8*RBP,		"8*RBP"					},
	{ 8*RBX,		"8*RBX"					},
	{ 8*R11,		"8*R11"					},
	{ 8*R10,		"8*R10"					},
	{ 8*R9,			"8*R9"					},
	{ 8*R8,			"8*R8"					},
	{ 8*RAX,		"8*RAX"					},
	{ 8*RCX,		"8*RCX"					},
	{ 8*RDX,		"8*RDX"					},
	{ 8*RSI,		"8*RSI"					},
	{ 8*RDI,		"8*RDI"					},
	{ 8*ORIG_RAX,		"8*ORIG_RAX"				},
	{ 8*RIP,		"8*RIP"					},
	{ 8*CS,			"8*CS"					},
	{ 8*EFLAGS,		"8*EFL"					},
	{ 8*RSP,		"8*RSP"					},
	{ 8*SS,			"8*SS"					},
#   elif defined(M68K)
	{ 4*PT_D1,		"4*PT_D1"				},
	{ 4*PT_D2,		"4*PT_D2"				},
	{ 4*PT_D3,		"4*PT_D3"				},
	{ 4*PT_D4,		"4*PT_D4"				},
	{ 4*PT_D5,		"4*PT_D5"				},
	{ 4*PT_D6,		"4*PT_D6"				},
	{ 4*PT_D7,		"4*PT_D7"				},
	{ 4*PT_A0,		"4*PT_A0"				},
	{ 4*PT_A1,		"4*PT_A1"				},
	{ 4*PT_A2,		"4*PT_A2"				},
	{ 4*PT_A3,		"4*PT_A3"				},
	{ 4*PT_A4,		"4*PT_A4"				},
	{ 4*PT_A5,		"4*PT_A5"				},
	{ 4*PT_A6,		"4*PT_A6"				},
	{ 4*PT_D0,		"4*PT_D0"				},
	{ 4*PT_USP,		"4*PT_USP"				},
	{ 4*PT_ORIG_D0,		"4*PT_ORIG_D0"				},
	{ 4*PT_SR,		"4*PT_SR"				},
	{ 4*PT_PC,		"4*PT_PC"				},
#   elif defined(SH)
	{ 4*REG_REG0,           "4*REG_REG0"                            },
	{ 4*(REG_REG0+1),       "4*REG_REG1"                            },
	{ 4*(REG_REG0+2),       "4*REG_REG2"                            },
	{ 4*(REG_REG0+3),       "4*REG_REG3"                            },
	{ 4*(REG_REG0+4),       "4*REG_REG4"                            },
	{ 4*(REG_REG0+5),       "4*REG_REG5"                            },
	{ 4*(REG_REG0+6),       "4*REG_REG6"                            },
	{ 4*(REG_REG0+7),       "4*REG_REG7"                            },
	{ 4*(REG_REG0+8),       "4*REG_REG8"                            },
	{ 4*(REG_REG0+9),       "4*REG_REG9"                            },
	{ 4*(REG_REG0+10),      "4*REG_REG10"                           },
	{ 4*(REG_REG0+11),      "4*REG_REG11"                           },
	{ 4*(REG_REG0+12),      "4*REG_REG12"                           },
	{ 4*(REG_REG0+13),      "4*REG_REG13"                           },
	{ 4*(REG_REG0+14),      "4*REG_REG14"                           },
	{ 4*REG_REG15,          "4*REG_REG15"                           },
	{ 4*REG_PC,             "4*REG_PC"                              },
	{ 4*REG_PR,             "4*REG_PR"                              },
	{ 4*REG_SR,             "4*REG_SR"                              },
	{ 4*REG_GBR,            "4*REG_GBR"                             },
	{ 4*REG_MACH,           "4*REG_MACH"                            },
	{ 4*REG_MACL,           "4*REG_MACL"                            },
	{ 4*REG_SYSCALL,        "4*REG_SYSCALL"                         },
	{ 4*REG_FPUL,           "4*REG_FPUL"                            },
	{ 4*REG_FPREG0,         "4*REG_FPREG0"                          },
	{ 4*(REG_FPREG0+1),     "4*REG_FPREG1"                          },
	{ 4*(REG_FPREG0+2),     "4*REG_FPREG2"                          },
	{ 4*(REG_FPREG0+3),     "4*REG_FPREG3"                          },
	{ 4*(REG_FPREG0+4),     "4*REG_FPREG4"                          },
	{ 4*(REG_FPREG0+5),     "4*REG_FPREG5"                          },
	{ 4*(REG_FPREG0+6),     "4*REG_FPREG6"                          },
	{ 4*(REG_FPREG0+7),     "4*REG_FPREG7"                          },
	{ 4*(REG_FPREG0+8),     "4*REG_FPREG8"                          },
	{ 4*(REG_FPREG0+9),     "4*REG_FPREG9"                          },
	{ 4*(REG_FPREG0+10),    "4*REG_FPREG10"                         },
	{ 4*(REG_FPREG0+11),    "4*REG_FPREG11"                         },
	{ 4*(REG_FPREG0+12),    "4*REG_FPREG12"                         },
	{ 4*(REG_FPREG0+13),    "4*REG_FPREG13"                         },
	{ 4*(REG_FPREG0+14),    "4*REG_FPREG14"                         },
	{ 4*REG_FPREG15,        "4*REG_FPREG15"                         },
#    ifdef REG_XDREG0
	{ 4*REG_XDREG0,         "4*REG_XDREG0"                          },
	{ 4*(REG_XDREG0+2),     "4*REG_XDREG2"                          },
	{ 4*(REG_XDREG0+4),     "4*REG_XDREG4"                          },
	{ 4*(REG_XDREG0+6),     "4*REG_XDREG6"                          },
	{ 4*(REG_XDREG0+8),     "4*REG_XDREG8"                          },
	{ 4*(REG_XDREG0+10),    "4*REG_XDREG10"                         },
	{ 4*(REG_XDREG0+12),    "4*REG_XDREG12"                         },
	{ 4*REG_XDREG14,        "4*REG_XDREG14"                         },
#    endif
	{ 4*REG_FPSCR,          "4*REG_FPSCR"                           },
#   elif defined(SH64)
	{ 0,		        "PC(L)"				        },
	{ 4,	                "PC(U)"				        },
	{ 8, 	                "SR(L)"	  	         		},
	{ 12,               	"SR(U)"     				},
	{ 16,            	"syscall no.(L)" 			},
	{ 20,            	"syscall_no.(U)"			},
	{ 24,            	"R0(L)"     				},
	{ 28,            	"R0(U)"     				},
	{ 32,            	"R1(L)"     				},
	{ 36,            	"R1(U)"     				},
	{ 40,            	"R2(L)"     				},
	{ 44,            	"R2(U)"     				},
	{ 48,            	"R3(L)"     				},
	{ 52,            	"R3(U)"     				},
	{ 56,            	"R4(L)"     				},
	{ 60,            	"R4(U)"     				},
	{ 64,            	"R5(L)"     				},
	{ 68,            	"R5(U)"     				},
	{ 72,            	"R6(L)"     				},
	{ 76,            	"R6(U)"     				},
	{ 80,            	"R7(L)"     				},
	{ 84,            	"R7(U)"     				},
	{ 88,            	"R8(L)"     				},
	{ 92,            	"R8(U)"     				},
	{ 96,            	"R9(L)"     				},
	{ 100,           	"R9(U)"     				},
	{ 104,           	"R10(L)"     				},
	{ 108,           	"R10(U)"     				},
	{ 112,           	"R11(L)"     				},
	{ 116,           	"R11(U)"     				},
	{ 120,           	"R12(L)"     				},
	{ 124,           	"R12(U)"     				},
	{ 128,           	"R13(L)"     				},
	{ 132,           	"R13(U)"     				},
	{ 136,           	"R14(L)"     				},
	{ 140,           	"R14(U)"     				},
	{ 144,           	"R15(L)"     				},
	{ 148,           	"R15(U)"     				},
	{ 152,           	"R16(L)"     				},
	{ 156,           	"R16(U)"     				},
	{ 160,           	"R17(L)"     				},
	{ 164,           	"R17(U)"     				},
	{ 168,           	"R18(L)"     				},
	{ 172,           	"R18(U)"     				},
	{ 176,           	"R19(L)"     				},
	{ 180,           	"R19(U)"     				},
	{ 184,           	"R20(L)"     				},
	{ 188,           	"R20(U)"     				},
	{ 192,           	"R21(L)"     				},
	{ 196,           	"R21(U)"     				},
	{ 200,           	"R22(L)"     				},
	{ 204,           	"R22(U)"     				},
	{ 208,           	"R23(L)"     				},
	{ 212,           	"R23(U)"     				},
	{ 216,           	"R24(L)"     				},
	{ 220,           	"R24(U)"     				},
	{ 224,           	"R25(L)"     				},
	{ 228,           	"R25(U)"     				},
	{ 232,           	"R26(L)"     				},
	{ 236,           	"R26(U)"     				},
	{ 240,           	"R27(L)"     				},
	{ 244,           	"R27(U)"     				},
	{ 248,           	"R28(L)"     				},
	{ 252,           	"R28(U)"     				},
	{ 256,           	"R29(L)"     				},
	{ 260,           	"R29(U)"     				},
	{ 264,           	"R30(L)"     				},
	{ 268,           	"R30(U)"     				},
	{ 272,           	"R31(L)"     				},
	{ 276,           	"R31(U)"     				},
	{ 280,           	"R32(L)"     				},
	{ 284,           	"R32(U)"     				},
	{ 288,           	"R33(L)"     				},
	{ 292,           	"R33(U)"     				},
	{ 296,           	"R34(L)"     				},
	{ 300,           	"R34(U)"     				},
	{ 304,           	"R35(L)"     				},
	{ 308,           	"R35(U)"     				},
	{ 312,           	"R36(L)"     				},
	{ 316,           	"R36(U)"     				},
	{ 320,           	"R37(L)"     				},
	{ 324,           	"R37(U)"     				},
	{ 328,           	"R38(L)"     				},
	{ 332,           	"R38(U)"     				},
	{ 336,           	"R39(L)"     				},
	{ 340,           	"R39(U)"     				},
	{ 344,           	"R40(L)"     				},
	{ 348,           	"R40(U)"     				},
	{ 352,           	"R41(L)"     				},
	{ 356,           	"R41(U)"     				},
	{ 360,           	"R42(L)"     				},
	{ 364,           	"R42(U)"     				},
	{ 368,           	"R43(L)"     				},
	{ 372,           	"R43(U)"     				},
	{ 376,           	"R44(L)"     				},
	{ 380,           	"R44(U)"     				},
	{ 384,           	"R45(L)"     				},
	{ 388,           	"R45(U)"     				},
	{ 392,           	"R46(L)"     				},
	{ 396,           	"R46(U)"     				},
	{ 400,           	"R47(L)"     				},
	{ 404,           	"R47(U)"     				},
	{ 408,           	"R48(L)"     				},
	{ 412,           	"R48(U)"     				},
	{ 416,           	"R49(L)"     				},
	{ 420,           	"R49(U)"     				},
	{ 424,           	"R50(L)"     				},
	{ 428,           	"R50(U)"     				},
	{ 432,           	"R51(L)"     				},
	{ 436,           	"R51(U)"     				},
	{ 440,           	"R52(L)"     				},
	{ 444,           	"R52(U)"     				},
	{ 448,           	"R53(L)"     				},
	{ 452,           	"R53(U)"     				},
	{ 456,           	"R54(L)"     				},
	{ 460,           	"R54(U)"     				},
	{ 464,           	"R55(L)"     				},
	{ 468,           	"R55(U)"     				},
	{ 472,           	"R56(L)"     				},
	{ 476,           	"R56(U)"     				},
	{ 480,           	"R57(L)"     				},
	{ 484,           	"R57(U)"     				},
	{ 488,           	"R58(L)"     				},
	{ 492,           	"R58(U)"     				},
	{ 496,           	"R59(L)"     				},
	{ 500,           	"R59(U)"     				},
	{ 504,           	"R60(L)"     				},
	{ 508,           	"R60(U)"     				},
	{ 512,           	"R61(L)"     				},
	{ 516,           	"R61(U)"     				},
	{ 520,           	"R62(L)"     				},
	{ 524,           	"R62(U)"     				},
	{ 528,                  "TR0(L)"                                },
	{ 532,                  "TR0(U)"                                },
	{ 536,                  "TR1(L)"                                },
	{ 540,                  "TR1(U)"                                },
	{ 544,                  "TR2(L)"                                },
	{ 548,                  "TR2(U)"                                },
	{ 552,                  "TR3(L)"                                },
	{ 556,                  "TR3(U)"                                },
	{ 560,                  "TR4(L)"                                },
	{ 564,                  "TR4(U)"                                },
	{ 568,                  "TR5(L)"                                },
	{ 572,                  "TR5(U)"                                },
	{ 576,                  "TR6(L)"                                },
	{ 580,                  "TR6(U)"                                },
	{ 584,                  "TR7(L)"                                },
	{ 588,                  "TR7(U)"                                },
	/* This entry is in case pt_regs contains dregs (depends on
	   the kernel build options). */
	{ uoff(regs),	        "offsetof(struct user, regs)"	        },
	{ uoff(fpu),	        "offsetof(struct user, fpu)"	        },
#   elif defined(ARM)
	{ uoff(regs.ARM_r0),	"r0"					},
	{ uoff(regs.ARM_r1),	"r1"					},
	{ uoff(regs.ARM_r2),	"r2"					},
	{ uoff(regs.ARM_r3),	"r3"					},
	{ uoff(regs.ARM_r4),	"r4"					},
	{ uoff(regs.ARM_r5),	"r5"					},
	{ uoff(regs.ARM_r6),	"r6"					},
	{ uoff(regs.ARM_r7),	"r7"					},
	{ uoff(regs.ARM_r8),	"r8"					},
	{ uoff(regs.ARM_r9),	"r9"					},
	{ uoff(regs.ARM_r10),	"r10"					},
	{ uoff(regs.ARM_fp),	"fp"					},
	{ uoff(regs.ARM_ip),	"ip"					},
	{ uoff(regs.ARM_sp),	"sp"					},
	{ uoff(regs.ARM_lr),	"lr"					},
	{ uoff(regs.ARM_pc),	"pc"					},
	{ uoff(regs.ARM_cpsr),	"cpsr"					},
#   elif defined(AVR32)
	{ uoff(regs.sr),	"sr"					},
	{ uoff(regs.pc),	"pc"					},
	{ uoff(regs.lr),	"lr"					},
	{ uoff(regs.sp),	"sp"					},
	{ uoff(regs.r12),	"r12"					},
	{ uoff(regs.r11),	"r11"					},
	{ uoff(regs.r10),	"r10"					},
	{ uoff(regs.r9),	"r9"					},
	{ uoff(regs.r8),	"r8"					},
	{ uoff(regs.r7),	"r7"					},
	{ uoff(regs.r6),	"r6"					},
	{ uoff(regs.r5),	"r5"					},
	{ uoff(regs.r4),	"r4"					},
	{ uoff(regs.r3),	"r3"					},
	{ uoff(regs.r2),	"r2"					},
	{ uoff(regs.r1),	"r1"					},
	{ uoff(regs.r0),	"r0"					},
	{ uoff(regs.r12_orig),	"orig_r12"				},
#   elif defined(MIPS)
	{ 0,			"r0"					},
	{ 1,			"r1"					},
	{ 2,			"r2"					},
	{ 3,			"r3"					},
	{ 4,			"r4"					},
	{ 5,			"r5"					},
	{ 6,			"r6"					},
	{ 7,			"r7"					},
	{ 8,			"r8"					},
	{ 9,			"r9"					},
	{ 10,			"r10"					},
	{ 11,			"r11"					},
	{ 12,			"r12"					},
	{ 13,			"r13"					},
	{ 14,			"r14"					},
	{ 15,			"r15"					},
	{ 16,			"r16"					},
	{ 17,			"r17"					},
	{ 18,			"r18"					},
	{ 19,			"r19"					},
	{ 20,			"r20"					},
	{ 21,			"r21"					},
	{ 22,			"r22"					},
	{ 23,			"r23"					},
	{ 24,			"r24"					},
	{ 25,			"r25"					},
	{ 26,			"r26"					},
	{ 27,			"r27"					},
	{ 28,			"r28"					},
	{ 29,			"r29"					},
	{ 30,			"r30"					},
	{ 31,			"r31"					},
	{ 32,			"f0"					},
	{ 33,			"f1"					},
	{ 34,			"f2"					},
	{ 35,			"f3"					},
	{ 36,			"f4"					},
	{ 37,			"f5"					},
	{ 38,			"f6"					},
	{ 39,			"f7"					},
	{ 40,			"f8"					},
	{ 41,			"f9"					},
	{ 42,			"f10"					},
	{ 43,			"f11"					},
	{ 44,			"f12"					},
	{ 45,			"f13"					},
	{ 46,			"f14"					},
	{ 47,			"f15"					},
	{ 48,			"f16"					},
	{ 49,			"f17"					},
	{ 50,			"f18"					},
	{ 51,			"f19"					},
	{ 52,			"f20"					},
	{ 53,			"f21"					},
	{ 54,			"f22"					},
	{ 55,			"f23"					},
	{ 56,			"f24"					},
	{ 57,			"f25"					},
	{ 58,			"f26"					},
	{ 59,			"f27"					},
	{ 60,			"f28"					},
	{ 61,			"f29"					},
	{ 62,			"f30"					},
	{ 63,			"f31"					},
	{ 64,			"pc"					},
	{ 65,			"cause"					},
	{ 66,			"badvaddr"				},
	{ 67,			"mmhi"					},
	{ 68,			"mmlo"					},
	{ 69,			"fpcsr"					},
	{ 70,			"fpeir"					},
#   elif defined(TILE)
	{ PTREGS_OFFSET_REG(0),  "r0"  },
	{ PTREGS_OFFSET_REG(1),  "r1"  },
	{ PTREGS_OFFSET_REG(2),  "r2"  },
	{ PTREGS_OFFSET_REG(3),  "r3"  },
	{ PTREGS_OFFSET_REG(4),  "r4"  },
	{ PTREGS_OFFSET_REG(5),  "r5"  },
	{ PTREGS_OFFSET_REG(6),  "r6"  },
	{ PTREGS_OFFSET_REG(7),  "r7"  },
	{ PTREGS_OFFSET_REG(8),  "r8"  },
	{ PTREGS_OFFSET_REG(9),  "r9"  },
	{ PTREGS_OFFSET_REG(10), "r10" },
	{ PTREGS_OFFSET_REG(11), "r11" },
	{ PTREGS_OFFSET_REG(12), "r12" },
	{ PTREGS_OFFSET_REG(13), "r13" },
	{ PTREGS_OFFSET_REG(14), "r14" },
	{ PTREGS_OFFSET_REG(15), "r15" },
	{ PTREGS_OFFSET_REG(16), "r16" },
	{ PTREGS_OFFSET_REG(17), "r17" },
	{ PTREGS_OFFSET_REG(18), "r18" },
	{ PTREGS_OFFSET_REG(19), "r19" },
	{ PTREGS_OFFSET_REG(20), "r20" },
	{ PTREGS_OFFSET_REG(21), "r21" },
	{ PTREGS_OFFSET_REG(22), "r22" },
	{ PTREGS_OFFSET_REG(23), "r23" },
	{ PTREGS_OFFSET_REG(24), "r24" },
	{ PTREGS_OFFSET_REG(25), "r25" },
	{ PTREGS_OFFSET_REG(26), "r26" },
	{ PTREGS_OFFSET_REG(27), "r27" },
	{ PTREGS_OFFSET_REG(28), "r28" },
	{ PTREGS_OFFSET_REG(29), "r29" },
	{ PTREGS_OFFSET_REG(30), "r30" },
	{ PTREGS_OFFSET_REG(31), "r31" },
	{ PTREGS_OFFSET_REG(32), "r32" },
	{ PTREGS_OFFSET_REG(33), "r33" },
	{ PTREGS_OFFSET_REG(34), "r34" },
	{ PTREGS_OFFSET_REG(35), "r35" },
	{ PTREGS_OFFSET_REG(36), "r36" },
	{ PTREGS_OFFSET_REG(37), "r37" },
	{ PTREGS_OFFSET_REG(38), "r38" },
	{ PTREGS_OFFSET_REG(39), "r39" },
	{ PTREGS_OFFSET_REG(40), "r40" },
	{ PTREGS_OFFSET_REG(41), "r41" },
	{ PTREGS_OFFSET_REG(42), "r42" },
	{ PTREGS_OFFSET_REG(43), "r43" },
	{ PTREGS_OFFSET_REG(44), "r44" },
	{ PTREGS_OFFSET_REG(45), "r45" },
	{ PTREGS_OFFSET_REG(46), "r46" },
	{ PTREGS_OFFSET_REG(47), "r47" },
	{ PTREGS_OFFSET_REG(48), "r48" },
	{ PTREGS_OFFSET_REG(49), "r49" },
	{ PTREGS_OFFSET_REG(50), "r50" },
	{ PTREGS_OFFSET_REG(51), "r51" },
	{ PTREGS_OFFSET_REG(52), "r52" },
	{ PTREGS_OFFSET_TP, "tp" },
	{ PTREGS_OFFSET_SP, "sp" },
	{ PTREGS_OFFSET_LR, "lr" },
	{ PTREGS_OFFSET_PC, "pc" },
	{ PTREGS_OFFSET_EX1, "ex1" },
	{ PTREGS_OFFSET_FAULTNUM, "faultnum" },
	{ PTREGS_OFFSET_ORIG_R0, "orig_r0" },
	{ PTREGS_OFFSET_FLAGS, "flags" },
#   endif
#   ifdef CRISV10
	{ 4*PT_FRAMETYPE, "4*PT_FRAMETYPE" },
	{ 4*PT_ORIG_R10, "4*PT_ORIG_R10" },
	{ 4*PT_R13, "4*PT_R13" },
	{ 4*PT_R12, "4*PT_R12" },
	{ 4*PT_R11, "4*PT_R11" },
	{ 4*PT_R10, "4*PT_R10" },
	{ 4*PT_R9, "4*PT_R9" },
	{ 4*PT_R8, "4*PT_R8" },
	{ 4*PT_R7, "4*PT_R7" },
	{ 4*PT_R6, "4*PT_R6" },
	{ 4*PT_R5, "4*PT_R5" },
	{ 4*PT_R4, "4*PT_R4" },
	{ 4*PT_R3, "4*PT_R3" },
	{ 4*PT_R2, "4*PT_R2" },
	{ 4*PT_R1, "4*PT_R1" },
	{ 4*PT_R0, "4*PT_R0" },
	{ 4*PT_MOF, "4*PT_MOF" },
	{ 4*PT_DCCR, "4*PT_DCCR" },
	{ 4*PT_SRP, "4*PT_SRP" },
	{ 4*PT_IRP, "4*PT_IRP" },
	{ 4*PT_CSRINSTR, "4*PT_CSRINSTR" },
	{ 4*PT_CSRADDR, "4*PT_CSRADDR" },
	{ 4*PT_CSRDATA, "4*PT_CSRDATA" },
	{ 4*PT_USP, "4*PT_USP" },
#   endif
#   ifdef CRISV32
	{ 4*PT_ORIG_R10, "4*PT_ORIG_R10" },
	{ 4*PT_R0, "4*PT_R0" },
	{ 4*PT_R1, "4*PT_R1" },
	{ 4*PT_R2, "4*PT_R2" },
	{ 4*PT_R3, "4*PT_R3" },
	{ 4*PT_R4, "4*PT_R4" },
	{ 4*PT_R5, "4*PT_R5" },
	{ 4*PT_R6, "4*PT_R6" },
	{ 4*PT_R7, "4*PT_R7" },
	{ 4*PT_R8, "4*PT_R8" },
	{ 4*PT_R9, "4*PT_R9" },
	{ 4*PT_R10, "4*PT_R10" },
	{ 4*PT_R11, "4*PT_R11" },
	{ 4*PT_R12, "4*PT_R12" },
	{ 4*PT_R13, "4*PT_R13" },
	{ 4*PT_ACR, "4*PT_ACR" },
	{ 4*PT_SRS, "4*PT_SRS" },
	{ 4*PT_MOF, "4*PT_MOF" },
	{ 4*PT_SPC, "4*PT_SPC" },
	{ 4*PT_CCS, "4*PT_CCS" },
	{ 4*PT_SRP, "4*PT_SRP" },
	{ 4*PT_ERP, "4*PT_ERP" },
	{ 4*PT_EXS, "4*PT_EXS" },
	{ 4*PT_EDA, "4*PT_EDA" },
	{ 4*PT_USP, "4*PT_USP" },
	{ 4*PT_PPC, "4*PT_PPC" },
	{ 4*PT_BP_CTRL, "4*PT_BP_CTRL" },
	{ 4*PT_BP+4, "4*PT_BP+4" },
	{ 4*PT_BP+8, "4*PT_BP+8" },
	{ 4*PT_BP+12, "4*PT_BP+12" },
	{ 4*PT_BP+16, "4*PT_BP+16" },
	{ 4*PT_BP+20, "4*PT_BP+20" },
	{ 4*PT_BP+24, "4*PT_BP+24" },
	{ 4*PT_BP+28, "4*PT_BP+28" },
	{ 4*PT_BP+32, "4*PT_BP+32" },
	{ 4*PT_BP+36, "4*PT_BP+36" },
	{ 4*PT_BP+40, "4*PT_BP+40" },
	{ 4*PT_BP+44, "4*PT_BP+44" },
	{ 4*PT_BP+48, "4*PT_BP+48" },
	{ 4*PT_BP+52, "4*PT_BP+52" },
	{ 4*PT_BP+56, "4*PT_BP+56" },
#   endif
#   ifdef MICROBLAZE
	{ PT_GPR(0),		"r0"					},
	{ PT_GPR(1),		"r1"					},
	{ PT_GPR(2),		"r2"					},
	{ PT_GPR(3),		"r3"					},
	{ PT_GPR(4),		"r4"					},
	{ PT_GPR(5),		"r5"					},
	{ PT_GPR(6),		"r6"					},
	{ PT_GPR(7),		"r7"					},
	{ PT_GPR(8),		"r8"					},
	{ PT_GPR(9),		"r9"					},
	{ PT_GPR(10),		"r10"					},
	{ PT_GPR(11),		"r11"					},
	{ PT_GPR(12),		"r12"					},
	{ PT_GPR(13),		"r13"					},
	{ PT_GPR(14),		"r14"					},
	{ PT_GPR(15),		"r15"					},
	{ PT_GPR(16),		"r16"					},
	{ PT_GPR(17),		"r17"					},
	{ PT_GPR(18),		"r18"					},
	{ PT_GPR(19),		"r19"					},
	{ PT_GPR(20),		"r20"					},
	{ PT_GPR(21),		"r21"					},
	{ PT_GPR(22),		"r22"					},
	{ PT_GPR(23),		"r23"					},
	{ PT_GPR(24),		"r24"					},
	{ PT_GPR(25),		"r25"					},
	{ PT_GPR(26),		"r26"					},
	{ PT_GPR(27),		"r27"					},
	{ PT_GPR(28),		"r28"					},
	{ PT_GPR(29),		"r29"					},
	{ PT_GPR(30),		"r30"					},
	{ PT_GPR(31),		"r31"					},
	{ PT_PC, 		"rpc",					},
	{ PT_MSR, 		"rmsr",					},
	{ PT_EAR,		"rear",					},
	{ PT_ESR,		"resr",					},
	{ PT_FSR,		"rfsr",					},
	{ PT_KERNEL_MODE, 	"kernel_mode",				},
#   endif

#   if !defined(SPARC) && !defined(HPPA) && !defined(POWERPC) \
		&& !defined(ALPHA) && !defined(IA64) \
		&& !defined(CRISV10) && !defined(CRISV32) && !defined(MICROBLAZE)
#    if !defined(S390) && !defined(S390X) && !defined(MIPS) && !defined(SPARC64) && !defined(AVR32) && !defined(BFIN) && !defined(TILE)
	{ uoff(u_fpvalid),	"offsetof(struct user, u_fpvalid)"	},
#    endif
#    if defined(I386) || defined(X86_64)
	{ uoff(i387),		"offsetof(struct user, i387)"		},
#    endif
#    if defined(M68K)
	{ uoff(m68kfp),		"offsetof(struct user, m68kfp)"		},
#    endif
	{ uoff(u_tsize),	"offsetof(struct user, u_tsize)"	},
	{ uoff(u_dsize),	"offsetof(struct user, u_dsize)"	},
	{ uoff(u_ssize),	"offsetof(struct user, u_ssize)"	},
#    if !defined(SPARC64)
	{ uoff(start_code),	"offsetof(struct user, start_code)"	},
#    endif
#    if defined(AVR32) || defined(SH64)
	{ uoff(start_data),	"offsetof(struct user, start_data)"	},
#    endif
#    if !defined(SPARC64)
	{ uoff(start_stack),	"offsetof(struct user, start_stack)"	},
#    endif
	{ uoff(signal),		"offsetof(struct user, signal)"		},
#    if !defined(AVR32) && !defined(S390) && !defined(S390X) && !defined(MIPS) && !defined(SH) && !defined(SH64) && !defined(SPARC64) && !defined(TILE)
	{ uoff(reserved),	"offsetof(struct user, reserved)"	},
#    endif
#    if !defined(SPARC64)
	{ uoff(u_ar0),		"offsetof(struct user, u_ar0)"		},
#    endif
#    if !defined(ARM) && !defined(AVR32) && !defined(MIPS) && !defined(S390) && !defined(S390X) && !defined(SPARC64) && !defined(BFIN) && !defined(TILE)
	{ uoff(u_fpstate),	"offsetof(struct user, u_fpstate)"	},
#    endif
	{ uoff(magic),		"offsetof(struct user, magic)"		},
	{ uoff(u_comm),		"offsetof(struct user, u_comm)"		},
#    if defined(I386) || defined(X86_64)
	{ uoff(u_debugreg),	"offsetof(struct user, u_debugreg)"	},
#    endif
#   endif /* !defined(many arches) */

#  endif /* LINUX */

#  ifdef SUNOS4
	{ uoff(u_pcb),		"offsetof(struct user, u_pcb)"		},
	{ uoff(u_procp),	"offsetof(struct user, u_procp)"	},
	{ uoff(u_ar0),		"offsetof(struct user, u_ar0)"		},
	{ uoff(u_comm[0]),	"offsetof(struct user, u_comm[0])"	},
	{ uoff(u_arg[0]),	"offsetof(struct user, u_arg[0])"	},
	{ uoff(u_ap),		"offsetof(struct user, u_ap)"		},
	{ uoff(u_qsave),	"offsetof(struct user, u_qsave)"	},
	{ uoff(u_rval1),	"offsetof(struct user, u_rval1)"	},
	{ uoff(u_rval2),	"offsetof(struct user, u_rval2)"	},
	{ uoff(u_error),	"offsetof(struct user, u_error)"	},
	{ uoff(u_eosys),	"offsetof(struct user, u_eosys)"	},
	{ uoff(u_ssave),	"offsetof(struct user, u_ssave)"	},
	{ uoff(u_signal[0]),	"offsetof(struct user, u_signal)"	},
	{ uoff(u_sigmask[0]),	"offsetof(struct user, u_sigmask)"	},
	{ uoff(u_sigonstack),	"offsetof(struct user, u_sigonstack)"	},
	{ uoff(u_sigintr),	"offsetof(struct user, u_sigintr)"	},
	{ uoff(u_sigreset),	"offsetof(struct user, u_sigreset)"	},
	{ uoff(u_oldmask),	"offsetof(struct user, u_oldmask)"	},
	{ uoff(u_code),		"offsetof(struct user, u_code)"		},
	{ uoff(u_addr),		"offsetof(struct user, u_addr)"		},
	{ uoff(u_sigstack),	"offsetof(struct user, u_sigstack)"	},
	{ uoff(u_ofile),	"offsetof(struct user, u_ofile)"	},
	{ uoff(u_pofile),	"offsetof(struct user, u_pofile)"	},
	{ uoff(u_ofile_arr[0]),	"offsetof(struct user, u_ofile_arr[0])"	},
	{ uoff(u_pofile_arr[0]),"offsetof(struct user, u_pofile_arr[0])"},
	{ uoff(u_lastfile),	"offsetof(struct user, u_lastfile)"	},
	{ uoff(u_cwd),		"offsetof(struct user, u_cwd)"		},
	{ uoff(u_cdir),		"offsetof(struct user, u_cdir)"		},
	{ uoff(u_rdir),		"offsetof(struct user, u_rdir)"		},
	{ uoff(u_cmask),	"offsetof(struct user, u_cmask)"	},
	{ uoff(u_ru),		"offsetof(struct user, u_ru)"		},
	{ uoff(u_cru),		"offsetof(struct user, u_cru)"		},
	{ uoff(u_timer[0]),	"offsetof(struct user, u_timer[0])"	},
	{ uoff(u_XXX[0]),	"offsetof(struct user, u_XXX[0])"	},
	{ uoff(u_ioch),		"offsetof(struct user, u_ioch)"		},
	{ uoff(u_start),	"offsetof(struct user, u_start)"	},
	{ uoff(u_acflag),	"offsetof(struct user, u_acflag)"	},
	{ uoff(u_prof.pr_base),	"offsetof(struct user, u_prof.pr_base)"	},
	{ uoff(u_prof.pr_size),	"offsetof(struct user, u_prof.pr_size)"	},
	{ uoff(u_prof.pr_off),	"offsetof(struct user, u_prof.pr_off)"	},
	{ uoff(u_prof.pr_scale),"offsetof(struct user, u_prof.pr_scale)"},
	{ uoff(u_rlimit[0]),	"offsetof(struct user, u_rlimit)"	},
	{ uoff(u_exdata.Ux_A),	"offsetof(struct user, u_exdata.Ux_A)"	},
	{ uoff(u_exdata.ux_shell[0]),"offsetof(struct user, u_exdata.ux_shell[0])"},
	{ uoff(u_lofault),	"offsetof(struct user, u_lofault)"	},
#  endif /* SUNOS4 */
#  ifndef HPPA
	{ sizeof(struct user),	"sizeof(struct user)"			},
#  endif
	{ 0,			NULL					},
};
# endif /* !FREEBSD */

int
sys_ptrace(struct tcb *tcp)
{
	const struct xlat *x;
	long addr;

	if (entering(tcp)) {
		printxval(ptrace_cmds, tcp->u_arg[0],
# ifndef FREEBSD
			  "PTRACE_???"
# else
			  "PT_???"
# endif
			);
		tprintf(", %lu, ", tcp->u_arg[1]);
		addr = tcp->u_arg[2];
# ifndef FREEBSD
		if (tcp->u_arg[0] == PTRACE_PEEKUSER
			|| tcp->u_arg[0] == PTRACE_POKEUSER) {
			for (x = struct_user_offsets; x->str; x++) {
				if (x->val >= addr)
					break;
			}
			if (!x->str)
				tprintf("%#lx, ", addr);
			else if (x->val > addr && x != struct_user_offsets) {
				x--;
				tprintf("%s + %ld, ", x->str, addr - x->val);
			}
			else
				tprintf("%s, ", x->str);
		}
		else
# endif
			tprintf("%#lx, ", tcp->u_arg[2]);
# ifdef LINUX
		switch (tcp->u_arg[0]) {
#  ifndef IA64
		case PTRACE_PEEKDATA:
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKUSER:
			break;
#  endif
		case PTRACE_CONT:
		case PTRACE_SINGLESTEP:
		case PTRACE_SYSCALL:
		case PTRACE_DETACH:
			printsignal(tcp->u_arg[3]);
			break;
#  ifdef PTRACE_SETOPTIONS
		case PTRACE_SETOPTIONS:
			printflags(ptrace_setoptions_flags, tcp->u_arg[3], "PTRACE_O_???");
			break;
#  endif
#  ifdef PTRACE_SETSIGINFO
		case PTRACE_SETSIGINFO: {
			siginfo_t si;
			if (!tcp->u_arg[3])
				tprintf("NULL");
			else if (syserror(tcp))
				tprintf("%#lx", tcp->u_arg[3]);
			else if (umove(tcp, tcp->u_arg[3], &si) < 0)
				tprintf("{???}");
			else
				printsiginfo(&si, verbose(tcp));
			break;
		}
#  endif
#  ifdef PTRACE_GETSIGINFO
		case PTRACE_GETSIGINFO:
			/* Don't print anything, do it at syscall return. */
			break;
#  endif
		default:
			tprintf("%#lx", tcp->u_arg[3]);
			break;
		}
	} else {
		switch (tcp->u_arg[0]) {
		case PTRACE_PEEKDATA:
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKUSER:
#  ifdef IA64
			return RVAL_HEX;
#  else
			printnum(tcp, tcp->u_arg[3], "%#lx");
			break;
#  endif
#  ifdef PTRACE_GETSIGINFO
		case PTRACE_GETSIGINFO: {
			siginfo_t si;
			if (!tcp->u_arg[3])
				tprintf("NULL");
			else if (syserror(tcp))
				tprintf("%#lx", tcp->u_arg[3]);
			else if (umove(tcp, tcp->u_arg[3], &si) < 0)
				tprintf("{???}");
			else
				printsiginfo(&si, verbose(tcp));
			break;
		}
#  endif
		}
	}
# endif /* LINUX */
# ifdef SUNOS4
		if (tcp->u_arg[0] == PTRACE_WRITEDATA ||
			tcp->u_arg[0] == PTRACE_WRITETEXT) {
			tprintf("%lu, ", tcp->u_arg[3]);
			printstr(tcp, tcp->u_arg[4], tcp->u_arg[3]);
		} else if (tcp->u_arg[0] != PTRACE_READDATA &&
				tcp->u_arg[0] != PTRACE_READTEXT) {
			tprintf("%#lx", tcp->u_arg[3]);
		}
	} else {
		if (tcp->u_arg[0] == PTRACE_READDATA ||
			tcp->u_arg[0] == PTRACE_READTEXT) {
			tprintf("%lu, ", tcp->u_arg[3]);
			printstr(tcp, tcp->u_arg[4], tcp->u_arg[3]);
		}
	}
# endif /* SUNOS4 */
# ifdef FREEBSD
		tprintf("%lu", tcp->u_arg[3]);
	}
# endif /* FREEBSD */
	return 0;
}

#endif /* !SVR4 */

#ifdef LINUX
# ifndef FUTEX_CMP_REQUEUE
#  define FUTEX_CMP_REQUEUE 4
# endif
# ifndef FUTEX_WAKE_OP
#  define FUTEX_WAKE_OP 5
# endif
# ifndef FUTEX_LOCK_PI
#  define FUTEX_LOCK_PI 6
#  define FUTEX_UNLOCK_PI 7
#  define FUTEX_TRYLOCK_PI 8
# endif
# ifndef FUTEX_WAIT_BITSET
#  define FUTEX_WAIT_BITSET 9
# endif
# ifndef FUTEX_WAKE_BITSET
#  define FUTEX_WAKE_BITSET 10
# endif
# ifndef FUTEX_WAIT_REQUEUE_PI
#  define FUTEX_WAIT_REQUEUE_PI 11
# endif
# ifndef FUTEX_CMP_REQUEUE_PI
#  define FUTEX_CMP_REQUEUE_PI 12
# endif
# ifndef FUTEX_PRIVATE_FLAG
#  define FUTEX_PRIVATE_FLAG 128
# endif
# ifndef FUTEX_CLOCK_REALTIME
#  define FUTEX_CLOCK_REALTIME 256
# endif
static const struct xlat futexops[] = {
	{ FUTEX_WAIT,					"FUTEX_WAIT" },
	{ FUTEX_WAKE,					"FUTEX_WAKE" },
	{ FUTEX_FD,					"FUTEX_FD" },
	{ FUTEX_REQUEUE,				"FUTEX_REQUEUE" },
	{ FUTEX_CMP_REQUEUE,				"FUTEX_CMP_REQUEUE" },
	{ FUTEX_WAKE_OP,				"FUTEX_WAKE_OP" },
	{ FUTEX_LOCK_PI,				"FUTEX_LOCK_PI" },
	{ FUTEX_UNLOCK_PI,				"FUTEX_UNLOCK_PI" },
	{ FUTEX_TRYLOCK_PI,				"FUTEX_TRYLOCK_PI" },
	{ FUTEX_WAIT_BITSET,				"FUTEX_WAIT_BITSET" },
	{ FUTEX_WAKE_BITSET,				"FUTEX_WAKE_BITSET" },
	{ FUTEX_WAIT_REQUEUE_PI,			"FUTEX_WAIT_REQUEUE_PI" },
	{ FUTEX_CMP_REQUEUE_PI,				"FUTEX_CMP_REQUEUE_PI" },
	{ FUTEX_WAIT|FUTEX_PRIVATE_FLAG,		"FUTEX_WAIT_PRIVATE" },
	{ FUTEX_WAKE|FUTEX_PRIVATE_FLAG,		"FUTEX_WAKE_PRIVATE" },
	{ FUTEX_FD|FUTEX_PRIVATE_FLAG,			"FUTEX_FD_PRIVATE" },
	{ FUTEX_REQUEUE|FUTEX_PRIVATE_FLAG,		"FUTEX_REQUEUE_PRIVATE" },
	{ FUTEX_CMP_REQUEUE|FUTEX_PRIVATE_FLAG,		"FUTEX_CMP_REQUEUE_PRIVATE" },
	{ FUTEX_WAKE_OP|FUTEX_PRIVATE_FLAG,		"FUTEX_WAKE_OP_PRIVATE" },
	{ FUTEX_LOCK_PI|FUTEX_PRIVATE_FLAG,		"FUTEX_LOCK_PI_PRIVATE" },
	{ FUTEX_UNLOCK_PI|FUTEX_PRIVATE_FLAG,		"FUTEX_UNLOCK_PI_PRIVATE" },
	{ FUTEX_TRYLOCK_PI|FUTEX_PRIVATE_FLAG,		"FUTEX_TRYLOCK_PI_PRIVATE" },
	{ FUTEX_WAIT_BITSET|FUTEX_PRIVATE_FLAG,		"FUTEX_WAIT_BITSET_PRIVATE" },
	{ FUTEX_WAKE_BITSET|FUTEX_PRIVATE_FLAG,		"FUTEX_WAKE_BITSET_PRIVATE" },
	{ FUTEX_WAIT_REQUEUE_PI|FUTEX_PRIVATE_FLAG,	"FUTEX_WAIT_REQUEUE_PI_PRIVATE" },
	{ FUTEX_CMP_REQUEUE_PI|FUTEX_PRIVATE_FLAG,	"FUTEX_CMP_REQUEUE_PI_PRIVATE" },
	{ FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME,	"FUTEX_WAIT_BITSET|FUTEX_CLOCK_REALTIME" },
	{ FUTEX_WAIT_BITSET|FUTEX_PRIVATE_FLAG|FUTEX_CLOCK_REALTIME,	"FUTEX_WAIT_BITSET_PRIVATE|FUTEX_CLOCK_REALTIME" },
	{ FUTEX_WAIT_REQUEUE_PI|FUTEX_CLOCK_REALTIME,	"FUTEX_WAIT_REQUEUE_PI|FUTEX_CLOCK_REALTIME" },
	{ FUTEX_WAIT_REQUEUE_PI|FUTEX_PRIVATE_FLAG|FUTEX_CLOCK_REALTIME,	"FUTEX_WAIT_REQUEUE_PI_PRIVATE|FUTEX_CLOCK_REALTIME" },
	{ 0,						NULL }
};
# ifndef FUTEX_OP_SET
#  define FUTEX_OP_SET		0
#  define FUTEX_OP_ADD		1
#  define FUTEX_OP_OR		2
#  define FUTEX_OP_ANDN		3
#  define FUTEX_OP_XOR		4
#  define FUTEX_OP_CMP_EQ	0
#  define FUTEX_OP_CMP_NE	1
#  define FUTEX_OP_CMP_LT	2
#  define FUTEX_OP_CMP_LE	3
#  define FUTEX_OP_CMP_GT	4
#  define FUTEX_OP_CMP_GE	5
# endif
static const struct xlat futexwakeops[] = {
	{ FUTEX_OP_SET,		"FUTEX_OP_SET" },
	{ FUTEX_OP_ADD,		"FUTEX_OP_ADD" },
	{ FUTEX_OP_OR,		"FUTEX_OP_OR" },
	{ FUTEX_OP_ANDN,	"FUTEX_OP_ANDN" },
	{ FUTEX_OP_XOR,		"FUTEX_OP_XOR" },
	{ 0,			NULL }
};
static const struct xlat futexwakecmps[] = {
	{ FUTEX_OP_CMP_EQ,	"FUTEX_OP_CMP_EQ" },
	{ FUTEX_OP_CMP_NE,	"FUTEX_OP_CMP_NE" },
	{ FUTEX_OP_CMP_LT,	"FUTEX_OP_CMP_LT" },
	{ FUTEX_OP_CMP_LE,	"FUTEX_OP_CMP_LE" },
	{ FUTEX_OP_CMP_GT,	"FUTEX_OP_CMP_GT" },
	{ FUTEX_OP_CMP_GE,	"FUTEX_OP_CMP_GE" },
	{ 0,			NULL }
};

int
sys_futex(struct tcb *tcp)
{
	if (entering(tcp)) {
		long int cmd = tcp->u_arg[1] & 127;
		tprintf("%p, ", (void *) tcp->u_arg[0]);
		printxval(futexops, tcp->u_arg[1], "FUTEX_???");
		tprintf(", %ld", tcp->u_arg[2]);
		if (cmd == FUTEX_WAKE_BITSET)
			tprintf(", %lx", tcp->u_arg[5]);
		else if (cmd == FUTEX_WAIT) {
			tprintf(", ");
			printtv(tcp, tcp->u_arg[3]);
		} else if (cmd == FUTEX_WAIT_BITSET) {
			tprintf(", ");
			printtv(tcp, tcp->u_arg[3]);
			tprintf(", %lx", tcp->u_arg[5]);
		} else if (cmd == FUTEX_REQUEUE)
			tprintf(", %ld, %p", tcp->u_arg[3], (void *) tcp->u_arg[4]);
		else if (cmd == FUTEX_CMP_REQUEUE || cmd == FUTEX_CMP_REQUEUE_PI)
			tprintf(", %ld, %p, %ld", tcp->u_arg[3], (void *) tcp->u_arg[4], tcp->u_arg[5]);
		else if (cmd == FUTEX_WAKE_OP) {
			tprintf(", %ld, %p, {", tcp->u_arg[3], (void *) tcp->u_arg[4]);
			if ((tcp->u_arg[5] >> 28) & 8)
				tprintf("FUTEX_OP_OPARG_SHIFT|");
			printxval(futexwakeops, (tcp->u_arg[5] >> 28) & 0x7, "FUTEX_OP_???");
			tprintf(", %ld, ", (tcp->u_arg[5] >> 12) & 0xfff);
			if ((tcp->u_arg[5] >> 24) & 8)
				tprintf("FUTEX_OP_OPARG_SHIFT|");
			printxval(futexwakecmps, (tcp->u_arg[5] >> 24) & 0x7, "FUTEX_OP_CMP_???");
			tprintf(", %ld}", tcp->u_arg[5] & 0xfff);
		} else if (cmd == FUTEX_WAIT_REQUEUE_PI) {
			tprintf(", ");
			printtv(tcp, tcp->u_arg[3]);
			tprintf(", %p", (void *) tcp->u_arg[4]);
		}
	}
	return 0;
}

static void
print_affinitylist(struct tcb *tcp, long list, unsigned int len)
{
	int first = 1;
	unsigned long w, min_len;

	if (abbrev(tcp) && len / sizeof(w) > max_strlen)
		min_len = len - max_strlen * sizeof(w);
	else
		min_len = 0;
	for (; len >= sizeof(w) && len > min_len;
	     len -= sizeof(w), list += sizeof(w)) {
		if (umove(tcp, list, &w) < 0)
			break;
		if (first)
			tprintf("{");
		else
			tprintf(", ");
		first = 0;
		tprintf("%lx", w);
	}
	if (len) {
		if (first)
			tprintf("%#lx", list);
		else
			tprintf(", %s}", (len >= sizeof(w) && len > min_len ?
				"???" : "..."));
	} else {
		tprintf(first ? "{}" : "}");
	}
}

int
sys_sched_setaffinity(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
		print_affinitylist(tcp, tcp->u_arg[2], tcp->u_arg[1]);
	}
	return 0;
}

int
sys_sched_getaffinity(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
	} else {
		if (tcp->u_rval == -1)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			print_affinitylist(tcp, tcp->u_arg[2], tcp->u_rval);
	}
	return 0;
}

static const struct xlat schedulers[] = {
	{ SCHED_OTHER,	"SCHED_OTHER" },
	{ SCHED_RR,	"SCHED_RR" },
	{ SCHED_FIFO,	"SCHED_FIFO" },
	{ 0,		NULL }
};

int
sys_sched_getscheduler(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%d", (int) tcp->u_arg[0]);
	} else if (! syserror(tcp)) {
		tcp->auxstr = xlookup (schedulers, tcp->u_rval);
		if (tcp->auxstr != NULL)
			return RVAL_STR;
	}
	return 0;
}

int
sys_sched_setscheduler(struct tcb *tcp)
{
	if (entering(tcp)) {
		struct sched_param p;
		tprintf("%d, ", (int) tcp->u_arg[0]);
		printxval(schedulers, tcp->u_arg[1], "SCHED_???");
		if (umove(tcp, tcp->u_arg[2], &p) < 0)
			tprintf(", %#lx", tcp->u_arg[2]);
		else
			tprintf(", { %d }", p.__sched_priority);
	}
	return 0;
}

int
sys_sched_getparam(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%d, ", (int) tcp->u_arg[0]);
	} else {
		struct sched_param p;
		if (umove(tcp, tcp->u_arg[1], &p) < 0)
			tprintf("%#lx", tcp->u_arg[1]);
		else
			tprintf("{ %d }", p.__sched_priority);
	}
	return 0;
}

int
sys_sched_setparam(struct tcb *tcp)
{
	if (entering(tcp)) {
		struct sched_param p;
		if (umove(tcp, tcp->u_arg[1], &p) < 0)
			tprintf("%d, %#lx", (int) tcp->u_arg[0], tcp->u_arg[1]);
		else
			tprintf("%d, { %d }", (int) tcp->u_arg[0], p.__sched_priority);
	}
	return 0;
}

int
sys_sched_get_priority_min(struct tcb *tcp)
{
	if (entering(tcp)) {
		printxval(schedulers, tcp->u_arg[0], "SCHED_???");
	}
	return 0;
}

# ifdef X86_64
# include <asm/prctl.h>

static const struct xlat archvals[] = {
	{ ARCH_SET_GS,		"ARCH_SET_GS"		},
	{ ARCH_SET_FS,		"ARCH_SET_FS"		},
	{ ARCH_GET_FS,		"ARCH_GET_FS"		},
	{ ARCH_GET_GS,		"ARCH_GET_GS"		},
	{ 0,			NULL			},
};

int
sys_arch_prctl(struct tcb *tcp)
{
	if (entering(tcp)) {
		printxval(archvals, tcp->u_arg[0], "ARCH_???");
		if (tcp->u_arg[0] == ARCH_SET_GS
		 || tcp->u_arg[0] == ARCH_SET_FS
		) {
			tprintf(", %#lx", tcp->u_arg[1]);
		}
	} else {
		if (tcp->u_arg[0] == ARCH_GET_GS
		 || tcp->u_arg[0] == ARCH_GET_FS
		) {
			long int v;
			if (!syserror(tcp) && umove(tcp, tcp->u_arg[1], &v) != -1)
				tprintf(", [%#lx]", v);
			else
				tprintf(", %#lx", tcp->u_arg[1]);
		}
	}
	return 0;
}
# endif /* X86_64 */


int
sys_getcpu(struct tcb *tcp)
{
	if (exiting(tcp)) {
		unsigned u;
		if (tcp->u_arg[0] == 0)
			tprintf("NULL, ");
		else if (umove(tcp, tcp->u_arg[0], &u) < 0)
			tprintf("%#lx, ", tcp->u_arg[0]);
		else
			tprintf("[%u], ", u);
		if (tcp->u_arg[1] == 0)
			tprintf("NULL, ");
		else if (umove(tcp, tcp->u_arg[1], &u) < 0)
			tprintf("%#lx, ", tcp->u_arg[1]);
		else
			tprintf("[%u], ", u);
		tprintf("%#lx", tcp->u_arg[2]);
	}
	return 0;
}

#endif /* LINUX */
