/* 
   Unix SMB/CIFS implementation.
   Critical Fault handling
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_BACKTRACE_SYMBOLS
#include <execinfo.h>
#endif
#include <atalk/logger.h>

#ifndef SIGNAL_CAST
#define SIGNAL_CAST (void (*)(int))
#endif
#ifndef SAFE_FREE /* Oh no this is also defined in tdb.h */
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); x=NULL;} } while(0)
#endif
#define BACKTRACE_STACK_SIZE 64

static void (*cont_fn)(void *);

/*******************************************************************
 Catch a signal. This should implement the following semantics:

 1) The handler remains installed after being called.
 2) The signal should be blocked during handler execution.
********************************************************************/

static void (*CatchSignal(int signum,void (*handler)(int )))(int)
{
#ifdef HAVE_SIGACTION
	struct sigaction act;
	struct sigaction oldact;

	ZERO_STRUCT(act);

	act.sa_handler = handler;
#if 0 
	/*
	 * We *want* SIGALRM to interrupt a system call.
	 */
	if(signum != SIGALRM)
		act.sa_flags = SA_RESTART;
#endif
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask,signum);
	sigaction(signum,&act,&oldact);
	return oldact.sa_handler;
#else /* !HAVE_SIGACTION */
	/* FIXME: need to handle sigvec and systems with broken signal() */
	return signal(signum, handler);
#endif
}

/*******************************************************************
 Something really nasty happened - panic !
********************************************************************/

void netatalk_panic(const char *why)
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;

	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	LOG(log_severe, logtype_default, "PANIC: %s", why);
	LOG(log_severe, logtype_default, "BACKTRACE: %d stack frames:", backtrace_size);
	
	if (backtrace_strings) {
		size_t i;

		for (i = 0; i < backtrace_size; i++)
			LOG(log_severe, logtype_default, " #%u %s", i, backtrace_strings[i]);

		SAFE_FREE(backtrace_strings);
	}
#endif
}


/*******************************************************************
report a fault
********************************************************************/
static void fault_report(int sig)
{
	static int counter;

	if (counter)
        abort();

	counter++;

	LOG(log_severe, logtype_default, "===============================================================");
	LOG(log_severe, logtype_default, "INTERNAL ERROR: Signal %d in pid %d (%s)",sig,(int)getpid(),VERSION);
	LOG(log_severe, logtype_default, "===============================================================");
  
	netatalk_panic("internal error");

	if (cont_fn) {
		cont_fn(NULL);
#ifdef SIGSEGV
		CatchSignal(SIGSEGV,SIGNAL_CAST SIG_DFL);
#endif
#ifdef SIGBUS
		CatchSignal(SIGBUS,SIGNAL_CAST SIG_DFL);
#endif
		return; /* this should cause a core dump */
	}
    abort();
}

/****************************************************************************
catch serious errors
****************************************************************************/
static void sig_fault(int sig)
{
	fault_report(sig);
}

/*******************************************************************
setup our fault handlers
********************************************************************/
void fault_setup(void (*fn)(void *))
{
	cont_fn = fn;

#ifdef SIGSEGV
	CatchSignal(SIGSEGV,SIGNAL_CAST sig_fault);
#endif
#ifdef SIGBUS
	CatchSignal(SIGBUS,SIGNAL_CAST sig_fault);
#endif
}

