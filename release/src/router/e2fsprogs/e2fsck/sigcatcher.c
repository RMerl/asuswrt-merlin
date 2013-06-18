/*
 * sigcatcher.c --- print a backtrace on a SIGSEGV, et. al
 *
 * Copyright (C) 2011 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "e2fsck.h"

struct str_table {
	int	num;
	const char	*name;
};

#define DEFINE_ENTRY(SYM)	{ SYM, #SYM },
#define END_TABLE		{ 0, 0 }

static struct str_table sig_table[] = {
#ifdef SIGHUP
	DEFINE_ENTRY(SIGHUP)
#endif
#ifdef SIGINT
	DEFINE_ENTRY(SIGINT)
#endif
#ifdef SIGQUIT
	DEFINE_ENTRY(SIGQUIT)
#endif
#ifdef SIGILL
	DEFINE_ENTRY(SIGILL)
#endif
#ifdef SIGTRAP
	DEFINE_ENTRY(SIGTRAP)
#endif
#ifdef SIGABRT
	DEFINE_ENTRY(SIGABRT)
#endif
#ifdef SIGIOT
	DEFINE_ENTRY(SIGIOT)
#endif
#ifdef SIGBUS
	DEFINE_ENTRY(SIGBUS)
#endif
#ifdef SIGFPE
	DEFINE_ENTRY(SIGFPE)
#endif
#ifdef SIGKILL
	DEFINE_ENTRY(SIGKILL)
#endif
#ifdef SIGUSR1
	DEFINE_ENTRY(SIGUSR1)
#endif
#ifdef SIGSEGV
	DEFINE_ENTRY(SIGSEGV)
#endif
#ifdef SIGUSR2
	DEFINE_ENTRY(SIGUSR2)
#endif
#ifdef SIGPIPE
	DEFINE_ENTRY(SIGPIPE)
#endif
#ifdef SIGALRM
	DEFINE_ENTRY(SIGALRM)
#endif
#ifdef SIGTERM
	DEFINE_ENTRY(SIGTERM)
#endif
#ifdef SIGSTKFLT
	DEFINE_ENTRY(SIGSTKFLT)
#endif
#ifdef SIGCHLD
	DEFINE_ENTRY(SIGCHLD)
#endif
#ifdef SIGCONT
	DEFINE_ENTRY(SIGCONT)
#endif
#ifdef SIGSTOP
	DEFINE_ENTRY(SIGSTOP)
#endif
#ifdef SIGTSTP
	DEFINE_ENTRY(SIGTSTP)
#endif
#ifdef SIGTTIN
	DEFINE_ENTRY(SIGTTIN)
#endif
#ifdef SIGTTOU
	DEFINE_ENTRY(SIGTTOU)
#endif
#ifdef SIGURG
	DEFINE_ENTRY(SIGURG)
#endif
#ifdef SIGXCPU
	DEFINE_ENTRY(SIGXCPU)
#endif
#ifdef SIGXFSZ
	DEFINE_ENTRY(SIGXFSZ)
#endif
#ifdef SIGVTALRM
	DEFINE_ENTRY(SIGVTALRM)
#endif
#ifdef SIGPROF
	DEFINE_ENTRY(SIGPROF)
#endif
#ifdef SIGWINCH
	DEFINE_ENTRY(SIGWINCH)
#endif
#ifdef SIGIO
	DEFINE_ENTRY(SIGIO)
#endif
#ifdef SIGPOLL
	DEFINE_ENTRY(SIGPOLL)
#endif
#ifdef SIGPWR
	DEFINE_ENTRY(SIGPWR)
#endif
#ifdef SIGSYS
	DEFINE_ENTRY(SIGSYS)
#endif
	END_TABLE
};

static struct str_table generic_code_table[] = {
#ifdef SI_ASYNCNL
	DEFINE_ENTRY(SI_ASYNCNL)
#endif
#ifdef SI_TKILL
	DEFINE_ENTRY(SI_TKILL)
#endif
#ifdef SI_SIGIO
	DEFINE_ENTRY(SI_SIGIO)
#endif
#ifdef SI_ASYNCIO
	DEFINE_ENTRY(SI_ASYNCIO)
#endif
#ifdef SI_MESGQ
	DEFINE_ENTRY(SI_MESGQ)
#endif
#ifdef SI_TIMER
	DEFINE_ENTRY(SI_TIMER)
#endif
#ifdef SI_QUEUE
	DEFINE_ENTRY(SI_QUEUE)
#endif
#ifdef SI_USER
	DEFINE_ENTRY(SI_USER)
#endif
#ifdef SI_KERNEL
	DEFINE_ENTRY(SI_KERNEL)
#endif
	END_TABLE
};

static struct str_table sigill_code_table[] = {
#ifdef ILL_ILLOPC
	DEFINE_ENTRY(ILL_ILLOPC)
#endif
#ifdef ILL_ILLOPN
	DEFINE_ENTRY(ILL_ILLOPN)
#endif
#ifdef ILL_ILLADR
	DEFINE_ENTRY(ILL_ILLADR)
#endif
#ifdef ILL_ILLTRP
	DEFINE_ENTRY(ILL_ILLTRP)
#endif
#ifdef ILL_PRVOPC
	DEFINE_ENTRY(ILL_PRVOPC)
#endif
#ifdef ILL_PRVREG
	DEFINE_ENTRY(ILL_PRVREG)
#endif
#ifdef ILL_COPROC
	DEFINE_ENTRY(ILL_COPROC)
#endif
#ifdef ILL_BADSTK
	DEFINE_ENTRY(ILL_BADSTK)
#endif
#ifdef BUS_ADRALN
	DEFINE_ENTRY(BUS_ADRALN)
#endif
#ifdef BUS_ADRERR
	DEFINE_ENTRY(BUS_ADRERR)
#endif
#ifdef BUS_OBJERR
	DEFINE_ENTRY(BUS_OBJERR)
#endif
	END_TABLE
};

static struct str_table sigfpe_code_table[] = {
#ifdef FPE_INTDIV
	DEFINE_ENTRY(FPE_INTDIV)
#endif
#ifdef FPE_INTOVF
	DEFINE_ENTRY(FPE_INTOVF)
#endif
#ifdef FPE_FLTDIV
	DEFINE_ENTRY(FPE_FLTDIV)
#endif
#ifdef FPE_FLTOVF
	DEFINE_ENTRY(FPE_FLTOVF)
#endif
#ifdef FPE_FLTUND
	DEFINE_ENTRY(FPE_FLTUND)
#endif
#ifdef FPE_FLTRES
	DEFINE_ENTRY(FPE_FLTRES)
#endif
#ifdef FPE_FLTINV
	DEFINE_ENTRY(FPE_FLTINV)
#endif
#ifdef FPE_FLTSUB
	DEFINE_ENTRY(FPE_FLTSUB)
#endif
	END_TABLE
};

static struct str_table sigsegv_code_table[] = {
#ifdef SEGV_MAPERR
	DEFINE_ENTRY(SEGV_MAPERR)
#endif
#ifdef SEGV_ACCERR
	DEFINE_ENTRY(SEGV_ACCERR)
#endif
	END_TABLE
};


static struct str_table sigbus_code_table[] = {
#ifdef BUS_ADRALN
	DEFINE_ENTRY(BUS_ADRALN)
#endif
#ifdef BUS_ADRERR
	DEFINE_ENTRY(BUS_ADRERR)
#endif
#ifdef BUS_OBJERR
	DEFINE_ENTRY(BUS_OBJERR)
#endif
	END_TABLE
};

#if 0 /* should this be hooked in somewhere? */
static struct str_table sigstrap_code_table[] = {
#ifdef TRAP_BRKPT
	DEFINE_ENTRY(TRAP_BRKPT)
#endif
#ifdef TRAP_TRACE
	DEFINE_ENTRY(TRAP_TRACE)
#endif
	END_TABLE
};
#endif

static struct str_table sigcld_code_table[] = {
#ifdef CLD_EXITED
	DEFINE_ENTRY(CLD_EXITED)
#endif
#ifdef CLD_KILLED
	DEFINE_ENTRY(CLD_KILLED)
#endif
#ifdef CLD_DUMPED
	DEFINE_ENTRY(CLD_DUMPED)
#endif
#ifdef CLD_TRAPPED
	DEFINE_ENTRY(CLD_TRAPPED)
#endif
#ifdef CLD_STOPPED
	DEFINE_ENTRY(CLD_STOPPED)
#endif
#ifdef CLD_CONTINUED
	DEFINE_ENTRY(CLD_CONTINUED)
#endif
	END_TABLE
};

#if 0 /* should this be hooked in somewhere? */
static struct str_table sigpoll_code_table[] = {
#ifdef POLL_IN
	DEFINE_ENTRY(POLL_IN)
#endif
#ifdef POLL_OUT
	DEFINE_ENTRY(POLL_OUT)
#endif
#ifdef POLL_MSG
	DEFINE_ENTRY(POLL_MSG)
#endif
#ifdef POLL_ERR
	DEFINE_ENTRY(POLL_ERR)
#endif
#ifdef POLL_PRI
	DEFINE_ENTRY(POLL_PRI)
#endif
#ifdef POLL_HUP
	DEFINE_ENTRY(POLL_HUP)
#endif
	END_TABLE
};
#endif

static const char *lookup_table(int num, struct str_table *table)
{
	struct str_table *p;

	for (p=table; p->name; p++)
		if (num == p->num)
			return(p->name);
	return NULL;
}

static const char *lookup_table_fallback(int num, struct str_table *table)
{
	static char buf[32];
	const char *ret = lookup_table(num, table);

	if (ret)
		return ret;
	snprintf(buf, sizeof(buf), "%d", num);
	buf[sizeof(buf)-1] = 0;
	return buf;
}

static void die_signal_handler(int signum, siginfo_t *siginfo, void *context)
{
       void *stack_syms[32];
       int frames;
       const char *cp;

       fprintf(stderr, "Signal (%d) %s ", signum,
	       lookup_table_fallback(signum, sig_table));
       if (siginfo->si_code == SI_USER)
	       fprintf(stderr, "(sent from pid %u) ", siginfo->si_pid);
       cp = lookup_table(siginfo->si_code, generic_code_table);
       if (cp)
	       fprintf(stderr, "si_code=%s ", cp);
       else if (signum == SIGILL)
	       fprintf(stderr, "si_code=%s ",
		       lookup_table_fallback(siginfo->si_code,
					     sigill_code_table));
       else if (signum == SIGFPE)
	       fprintf(stderr, "si_code=%s ",
		       lookup_table_fallback(siginfo->si_code,
					     sigfpe_code_table));
       else if (signum == SIGSEGV)
	       fprintf(stderr, "si_code=%s ",
		       lookup_table_fallback(siginfo->si_code,
					     sigsegv_code_table));
       else if (signum == SIGBUS)
	       fprintf(stderr, "si_code=%s ",
		       lookup_table_fallback(siginfo->si_code,
					     sigbus_code_table));
       else if (signum == SIGCHLD)
	       fprintf(stderr, "si_code=%s ",
		       lookup_table_fallback(siginfo->si_code,
					     sigcld_code_table));
       else
	       fprintf(stderr, "si code=%d ", siginfo->si_code);
       if ((siginfo->si_code != SI_USER) &&
	   (signum == SIGILL || signum == SIGFPE ||
	    signum == SIGSEGV || signum == SIGBUS))
	       fprintf(stderr, "fault addr=%p", siginfo->si_addr);
       fprintf(stderr, "\n");

#ifdef HAVE_BACKTRACE
       frames = backtrace(stack_syms, 32);
       backtrace_symbols_fd(stack_syms, frames, 2);
#endif
       exit(FSCK_ERROR);
}

void sigcatcher_setup(void)
{
	struct sigaction	sa;
	
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = die_signal_handler;
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGFPE, &sa, 0);
	sigaction(SIGILL, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
	sigaction(SIGSEGV, &sa, 0);
}	


#ifdef DEBUG
#include <getopt.h>

void usage(void)
{
	fprintf(stderr, "tst_sigcatcher: [-akfn]\n");
	exit(1);
}

int main(int argc, char** argv)
{
	struct sigaction	sa;
	char			*p = 0;
	int 			i, c;
	volatile		x=0;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = die_signal_handler;
	sa.sa_flags = SA_SIGINFO;
	for (i=1; i < 31; i++)
		sigaction(i, &sa, 0);

	while ((c = getopt (argc, argv, "afkn")) != EOF)
		switch (c) {
		case 'a':
			abort();
			break;
		case 'f':
			printf("%d\n", 42/x);
		case 'k':
			kill(getpid(), SIGTERM);
			break;
		case 'n':
			*p = 42;
		default:
			usage ();
		}

	printf("Sleeping for 10 seconds, send kill signal to pid %u...\n",
	       getpid());
	fflush(stdout);
	sleep(10);
	exit(0);
}
#endif
