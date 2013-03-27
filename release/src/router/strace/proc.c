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

#include "defs.h"

#ifdef SVR4
#ifndef HAVE_MP_PROCFS

static const struct xlat proc_status_flags[] = {
	{ PR_STOPPED,	"PR_STOPPED"	},
	{ PR_ISTOP,	"PR_ISTOP"	},
	{ PR_DSTOP,	"PR_DSTOP"	},
	{ PR_ASLEEP,	"PR_ASLEEP"	},
	{ PR_FORK,	"PR_FORK"	},
	{ PR_RLC,	"PR_RLC"	},
	{ PR_PTRACE,	"PR_PTRACE"	},
	{ PR_PCINVAL,	"PR_PCINVAL"	},
	{ PR_ISSYS,	"PR_ISSYS"	},
#ifdef PR_STEP
	{ PR_STEP,	"PR_STEP"	},
#endif
#ifdef PR_KLC
	{ PR_KLC,	"PR_KLC"	},
#endif
#ifdef PR_ASYNC
	{ PR_ASYNC,	"PR_ASYNC"	},
#endif
#ifdef PR_PCOMPAT
	{ PR_PCOMPAT,	"PR_PCOMPAT"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat proc_status_why[] = {
	{ PR_REQUESTED,	"PR_REQUESTED"	},
	{ PR_SIGNALLED,	"PR_SIGNALLED"	},
	{ PR_SYSENTRY,	"PR_SYSENTRY"	},
	{ PR_SYSEXIT,	"PR_SYSEXIT"	},
	{ PR_JOBCONTROL,"PR_JOBCONTROL"	},
	{ PR_FAULTED,	"PR_FAULTED"	},
#ifdef PR_SUSPENDED
	{ PR_SUSPENDED,	"PR_SUSPENDED"	},
#endif
#ifdef PR_CHECKPOINT
	{ PR_CHECKPOINT,"PR_CHECKPOINT"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat proc_run_flags[] = {
	{ PRCSIG,	"PRCSIG"	},
	{ PRCFAULT,	"PRCFAULT"	},
	{ PRSTRACE,	"PRSTRACE"	},
	{ PRSHOLD,	"PRSHOLD"	},
	{ PRSFAULT,	"PRSFAULT"	},
	{ PRSVADDR,	"PRSVADDR"	},
	{ PRSTEP,	"PRSTEP"	},
	{ PRSABORT,	"PRSABORT"	},
	{ PRSTOP,	"PRSTOP"	},
	{ 0,		NULL		},
};

int
proc_ioctl(tcp, code, arg)
struct tcb *tcp;
int code, arg;
{
	int val;
	prstatus_t status;
	prrun_t run;

	if (entering(tcp))
		return 0;

	switch (code) {
	case PIOCSTATUS:
	case PIOCSTOP:
	case PIOCWSTOP:
		if (arg == 0)
			tprintf(", NULL");
		else if (syserror(tcp))
			tprintf(", %#x", arg);
		else if (umove(tcp, arg, &status) < 0)
			tprintf(", {...}");
		else {
			tprintf(", {pr_flags=");
			printflags(proc_status_flags, status.pr_flags, "PR_???");
			if (status.pr_why) {
				tprintf(", pr_why=");
				printxval(proc_status_why, status.pr_why,
					  "PR_???");
			}
			switch (status.pr_why) {
			case PR_SIGNALLED:
			case PR_JOBCONTROL:
				tprintf(", pr_what=");
				printsignal(status.pr_what);
				break;
			case PR_FAULTED:
				tprintf(", pr_what=%d", status.pr_what);
				break;
			case PR_SYSENTRY:
			case PR_SYSEXIT:
				tprintf(", pr_what=SYS_%s",
					sysent[status.pr_what].sys_name);
				break;
			}
			tprintf(", ...}");
		}
		return 1;
	case PIOCRUN:
		if (arg == 0)
			tprintf(", NULL");
		else if (umove(tcp, arg, &run) < 0)
			tprintf(", {...}");
		else {
			tprintf(", {pr_flags=");
			printflags(proc_run_flags, run.pr_flags, "PR???");
			tprintf(", ...}");
		}
		return 1;
#ifdef PIOCSET
	case PIOCSET:
	case PIOCRESET:
		if (umove(tcp, arg, &val) < 0)
			tprintf(", [?]");
		else {
			tprintf(", [");
			printflags(proc_status_flags, val, "PR_???");
			tprintf("]");
		}
		return 1;
#endif /* PIOCSET */
	case PIOCKILL:
	case PIOCUNKILL:
		/* takes a pointer to a signal */
		if (umove(tcp, arg, &val) < 0)
			tprintf(", [?]");
		else {
			tprintf(", [");
			printsignal(val);
			tprintf("]");
		}
		return 1;
	case PIOCSFORK:
	case PIOCRFORK:
	case PIOCSRLC:
	case PIOCRRLC:
		/* doesn't take an arg */
		return 1;
	default:
		/* ad naseum */
		return 0;
	}
}

#endif /* HAVE_MP_PROCFS */
#endif /* SVR4 */

#ifdef FREEBSD
#include <sys/pioctl.h>

static const struct xlat proc_status_why[] = {
	{ S_EXEC,	"S_EXEC"	},
	{ S_SIG,	"S_SIG"		},
	{ S_SCE,	"S_SCE"		},
	{ S_SCX,	"S_SCX"		},
	{ S_CORE,	"S_CORE"	},
	{ S_EXIT,	"S_EXIT"	},
	{ 0,		NULL		}
};

static const struct xlat proc_status_flags[] = {
	{ PF_LINGER,	"PF_LINGER"	},
	{ PF_ISUGID,	"PF_ISUGID"	},
	{ 0,		NULL		}
};

int
proc_ioctl(tcp, code, arg)
struct tcb *tcp;
int code, arg;
{
	int val;
	struct procfs_status status;

	if (entering(tcp))
		return 0;

	switch (code) {
	case PIOCSTATUS:
	case PIOCWAIT:
		if (arg == 0)
			tprintf(", NULL");
		else if (syserror(tcp))
			tprintf(", %x", arg);
		else if (umove(tcp, arg, &status) < 0)
			tprintf(", {...}");
		else {
			tprintf(", {state=%d, flags=", status.state);
			printflags(proc_status_flags, status.flags, "PF_???");
			tprintf(", events=");
			printflags(proc_status_why, status.events, "S_???");
			tprintf(", why=");
			printxval(proc_status_why, status.why, "S_???");
			tprintf(", val=%lu}", status.val);
		}
		return 1;
	case PIOCBIS:
		tprintf(", ");
		printflags(proc_status_why, arg, "S_???");
		return 1;
		return 1;
	case PIOCSFL:
		tprintf(", ");
		printflags(proc_status_flags, arg, "PF_???");
		return 1;
	case PIOCGFL:
		if (syserror(tcp))
			tprintf(", %#x", arg);
		else if (umove(tcp, arg, &val) < 0)
			tprintf(", {...}");
		else {
			tprintf(", [");
			printflags(proc_status_flags, val, "PF_???");
			tprintf("]");
		}
		return 1;
	default:
		/* ad naseum */
		return 0;
	}
}
#endif
