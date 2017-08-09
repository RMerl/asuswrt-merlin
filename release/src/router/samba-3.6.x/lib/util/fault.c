/* 
   Unix SMB/CIFS implementation.
   Critical Fault handling
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "version.h"
#include "system/wait.h"
#include "system/filesys.h"

/**
 * @file
 * @brief Fault handling
 */

/* the registered fault handler */
static struct {
	const char *name;
	void (*fault_handler)(int sig);
} fault_handlers;

static const char *progname;

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#elif HAVE_LIBEXC_H
#include <libexc.h>
#endif

/**
 * Write backtrace to debug log
 */
_PUBLIC_ void call_backtrace(void)
{
#ifdef HAVE_BACKTRACE
#ifndef BACKTRACE_STACK_SIZE
#define BACKTRACE_STACK_SIZE 64
#endif
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	int backtrace_size;
	char **backtrace_strings;

	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	DEBUG(0, ("BACKTRACE: %lu stack frames:\n", 
		  (unsigned long)backtrace_size));
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			DEBUGADD(0, (" #%u %s\n", i, backtrace_strings[i]));

		/* Leak the backtrace_strings, rather than risk what free() might do */
	}

#elif HAVE_LIBEXC

#define NAMESIZE 32 /* Arbitrary */
#ifndef BACKTRACE_STACK_SIZE
#define BACKTRACE_STACK_SIZE 64
#endif

	/* The IRIX libexc library provides an API for unwinding the stack. See
	 * libexc(3) for details. Apparantly trace_back_stack leaks memory, but
	 * since we are about to abort anyway, it hardly matters.
	 *
	 * Note that if we paniced due to a SIGSEGV or SIGBUS (or similar) this
	 * will fail with a nasty message upon failing to open the /proc entry.
	 */
	{
		uint64_t	addrs[BACKTRACE_STACK_SIZE];
		char *      	names[BACKTRACE_STACK_SIZE];
		char		namebuf[BACKTRACE_STACK_SIZE * NAMESIZE];

		int		i;
		int		levels;

		ZERO_ARRAY(addrs);
		ZERO_ARRAY(names);
		ZERO_ARRAY(namebuf);

		for (i = 0; i < BACKTRACE_STACK_SIZE; i++) {
			names[i] = namebuf + (i * NAMESIZE);
		}

		levels = trace_back_stack(0, addrs, names,
				BACKTRACE_STACK_SIZE, NAMESIZE);

		DEBUG(0, ("BACKTRACE: %d stack frames:\n", levels));
		for (i = 0; i < levels; i++) {
			DEBUGADD(0, (" #%d 0x%llx %s\n", i, addrs[i], names[i]));
		}
     }
#undef NAMESIZE
#endif
}

_PUBLIC_ const char *panic_action = NULL;

/**
 Something really nasty happened - panic !
**/
_PUBLIC_ _NORETURN_ void smb_panic(const char *why)
{
	int result;

	if (panic_action && *panic_action) {
		char pidstr[20];
		char cmdstring[200];
		safe_strcpy(cmdstring, panic_action, sizeof(cmdstring));
		snprintf(pidstr, sizeof(pidstr), "%d", (int) getpid());
		all_string_sub(cmdstring, "%PID%", pidstr, sizeof(cmdstring));
		if (progname) {
			all_string_sub(cmdstring, "%PROG%", progname, sizeof(cmdstring));
		}
		DEBUG(0, ("smb_panic(): calling panic action [%s]\n", cmdstring));
		result = system(cmdstring);

		if (result == -1)
			DEBUG(0, ("smb_panic(): fork failed in panic action: %s\n",
				  strerror(errno)));
		else
			DEBUG(0, ("smb_panic(): action returned status %d\n",
				  WEXITSTATUS(result)));
	}
	DEBUG(0,("PANIC: %s\n", why));

	call_backtrace();

#ifdef SIGABRT
	CatchSignal(SIGABRT, SIG_DFL);
#endif
	abort();
}

/**
report a fault
**/
_NORETURN_ static void fault_report(int sig)
{
	static int counter;
	
	if (counter) _exit(1);

	DEBUG(0,("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n"));
	DEBUG(0,("INTERNAL ERROR: Signal %d in pid %d (%s)",sig,(int)getpid(),SAMBA_VERSION_STRING));
	DEBUG(0,("\nPlease read the file BUGS.txt in the distribution\n"));
	DEBUG(0,("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n"));

	smb_panic("internal error");

	exit(1);
}

/**
catch serious errors
**/
_NORETURN_ static void sig_fault(int sig)
{
	if (fault_handlers.fault_handler) {
		/* we have a fault handler, call it. It may not return. */
		fault_handlers.fault_handler(sig);
	}
	/* If it returns or doesn't exist, use regular reporter */
	fault_report(sig);
}

/**
setup our fault handlers
**/
_PUBLIC_ void fault_setup(const char *pname)
{
	if (progname != NULL) {
		return;
	}
	progname = pname;
#ifdef SIGSEGV
	CatchSignal(SIGSEGV, sig_fault);
#endif
#ifdef SIGBUS
	CatchSignal(SIGBUS, sig_fault);
#endif
#ifdef SIGABRT
	CatchSignal(SIGABRT, sig_fault);
#endif
#ifdef SIGFPE
	CatchSignal(SIGFPE, sig_fault);
#endif
}

/**
   disable setting up fault handlers
**/
_PUBLIC_ void fault_setup_disable(void)
{
	progname = "fault disabled";
}


/**
  register a fault handler. 
  Should only be called once in the execution of smbd.
*/
_PUBLIC_ bool register_fault_handler(const char *name, 
				     void (*fault_handler)(int sig))
{
	if (fault_handlers.name != NULL) {
		/* it's already registered! */
		DEBUG(2,("fault handler '%s' already registered - failed '%s'\n", 
			 fault_handlers.name, name));
		return false;
	}

	fault_handlers.name = name;
	fault_handlers.fault_handler = fault_handler;

	DEBUG(2,("fault handler '%s' registered\n", name));
	return true;
}
