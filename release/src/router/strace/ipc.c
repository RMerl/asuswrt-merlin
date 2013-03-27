/*
 * Copyright (c) 1993 Ulrich Pegelow <pegelow@moorea.uni-muenster.de>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
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

#if defined(LINUX) || defined(SUNOS4) || defined(FREEBSD)

# ifdef HAVE_MQUEUE_H
#  include <mqueue.h>
# endif

#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>

#ifndef MSG_STAT
#define MSG_STAT 11
#endif
#ifndef MSG_INFO
#define MSG_INFO 12
#endif
#ifndef SHM_STAT
#define SHM_STAT 13
#endif
#ifndef SHM_INFO
#define SHM_INFO 14
#endif
#ifndef SEM_STAT
#define SEM_STAT 18
#endif
#ifndef SEM_INFO
#define SEM_INFO 19
#endif

#if defined LINUX && !defined IPC_64
# define IPC_64 0x100
#endif

extern void printsigevent(struct tcb *tcp, long arg);

static const struct xlat msgctl_flags[] = {
	{ IPC_RMID,	"IPC_RMID"	},
	{ IPC_SET,	"IPC_SET"	},
	{ IPC_STAT,	"IPC_STAT"	},
#ifdef LINUX
	{ IPC_INFO,	"IPC_INFO"	},
	{ MSG_STAT,	"MSG_STAT"	},
	{ MSG_INFO,	"MSG_INFO"	},
#endif /* LINUX */
	{ 0,		NULL		},
};

static const struct xlat semctl_flags[] = {
	{ IPC_RMID,	"IPC_RMID"	},
	{ IPC_SET,	"IPC_SET"	},
	{ IPC_STAT,	"IPC_STAT"	},
#ifdef LINUX
	{ IPC_INFO,	"IPC_INFO"	},
	{ SEM_STAT,	"SEM_STAT"	},
	{ SEM_INFO,	"SEM_INFO"	},
#endif /* LINUX */
	{ GETPID,	"GETPID"	},
	{ GETVAL,	"GETVAL"	},
	{ GETALL,	"GETALL"	},
	{ GETNCNT,	"GETNCNT"	},
	{ GETZCNT,	"GETZCNT"	},
	{ SETVAL,	"SETVAL"	},
	{ SETALL,	"SETALL"	},
	{ 0,		NULL		},
};

static const struct xlat shmctl_flags[] = {
	{ IPC_RMID,	"IPC_RMID"	},
	{ IPC_SET,	"IPC_SET"	},
	{ IPC_STAT,	"IPC_STAT"	},
#ifdef LINUX
	{ IPC_INFO,	"IPC_INFO"	},
	{ SHM_STAT,	"SHM_STAT"	},
	{ SHM_INFO,	"SHM_INFO"	},
#endif /* LINUX */
#ifdef SHM_LOCK
	{ SHM_LOCK,	"SHM_LOCK"	},
#endif
#ifdef SHM_UNLOCK
	{ SHM_UNLOCK,	"SHM_UNLOCK"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat resource_flags[] = {
	{ IPC_CREAT,	"IPC_CREAT"	},
	{ IPC_EXCL,	"IPC_EXCL"	},
	{ IPC_NOWAIT,	"IPC_NOWAIT"	},
	{ 0,		NULL		},
};

static const struct xlat shm_resource_flags[] = {
	{ IPC_CREAT,	"IPC_CREAT"	},
	{ IPC_EXCL,	"IPC_EXCL"	},
#ifdef SHM_HUGETLB
	{ SHM_HUGETLB,	"SHM_HUGETLB"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat shm_flags[] = {
#ifdef LINUX
	{ SHM_REMAP,	"SHM_REMAP"	},
#endif /* LINUX */
	{ SHM_RDONLY,	"SHM_RDONLY"	},
	{ SHM_RND,	"SHM_RND"	},
	{ 0,		NULL		},
};

static const struct xlat msg_flags[] = {
	{ MSG_NOERROR,	"MSG_NOERROR"	},
#ifdef LINUX
	{ MSG_EXCEPT,	"MSG_EXCEPT"	},
#endif /* LINUX */
	{ IPC_NOWAIT,	"IPC_NOWAIT"	},
	{ 0,		NULL		},
};

static const struct xlat semop_flags[] = {
	{ SEM_UNDO,	"SEM_UNDO"	},
	{ IPC_NOWAIT,	"IPC_NOWAIT"	},
	{ 0,		NULL		},
};

int sys_msgget(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		if (tcp->u_arg[0])
			tprintf("%#lx", tcp->u_arg[0]);
		else
			tprintf("IPC_PRIVATE");
		tprintf(", ");
		if (printflags(resource_flags, tcp->u_arg[1] & ~0777, NULL) != 0)
			tprintf("|");
		tprintf("%#lo", tcp->u_arg[1] & 0777);
	}
	return 0;
}

#ifdef IPC_64
# define PRINTCTL(flagset, arg, dflt) \
	if ((arg) & IPC_64) tprintf("IPC_64|"); \
	printxval((flagset), (arg) &~ IPC_64, dflt)
#else
# define PRINTCTL printxval
#endif

static int
indirect_ipccall(tcp)
struct tcb *tcp;
{
#ifdef LINUX
#ifdef X86_64
	return current_personality > 0;
#endif
#if defined IA64
	return tcp->scno < 1024; /* ia32 emulation syscalls are low */
#endif
#if !defined MIPS && !defined HPPA
	return 1;
#endif
#endif	/* LINUX */
	return 0;
}

int sys_msgctl(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
		PRINTCTL(msgctl_flags, tcp->u_arg[1], "MSG_???");
		tprintf(", %#lx", tcp->u_arg[indirect_ipccall(tcp) ? 3 : 2]);
	}
	return 0;
}

static void
tprint_msgsnd(struct tcb *tcp, long addr, unsigned long count,
	      unsigned long flags)
{
	long mtype;

	if (umove(tcp, addr, &mtype) < 0) {
		tprintf("%#lx", addr);
	} else {
		tprintf("{%lu, ", mtype);
		printstr(tcp, addr + sizeof(mtype), count);
		tprintf("}");
	}
	tprintf(", %lu, ", count);
	printflags(msg_flags, flags, "MSG_???");
}

int sys_msgsnd(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%d, ", (int) tcp->u_arg[0]);
		if (indirect_ipccall(tcp)) {
			tprint_msgsnd(tcp, tcp->u_arg[3], tcp->u_arg[1],
				      tcp->u_arg[2]);
		} else {
			tprint_msgsnd(tcp, tcp->u_arg[1], tcp->u_arg[2],
				      tcp->u_arg[3]);
		}
	}
	return 0;
}

static void
tprint_msgrcv(struct tcb *tcp, long addr, unsigned long count, long msgtyp)
{
	long mtype;

	if (syserror(tcp) || umove(tcp, addr, &mtype) < 0) {
		tprintf("%#lx", addr);
	} else {
		tprintf("{%lu, ", mtype);
		printstr(tcp, addr + sizeof(mtype), count);
		tprintf("}");
	}
	tprintf(", %lu, %ld, ", count, msgtyp);
}

int sys_msgrcv(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%d, ", (int) tcp->u_arg[0]);
	} else {
		if (indirect_ipccall(tcp)) {
			struct ipc_wrapper {
				struct msgbuf *msgp;
				long msgtyp;
			} tmp;

			if (umove(tcp, tcp->u_arg[3], &tmp) < 0) {
				tprintf("%#lx, %lu, ",
					tcp->u_arg[3], tcp->u_arg[1]);
			} else {
				tprint_msgrcv(tcp, (long) tmp.msgp,
					tcp->u_arg[1], tmp.msgtyp);
			}
			printflags(msg_flags, tcp->u_arg[2], "MSG_???");
		} else {
			tprint_msgrcv(tcp, tcp->u_arg[1],
				tcp->u_arg[2], tcp->u_arg[3]);
			printflags(msg_flags, tcp->u_arg[4], "MSG_???");
		}
	}
	return 0;
}

static void
tprint_sembuf(struct tcb *tcp, long addr, unsigned long count)
{
	unsigned long i, max_count;

	if (abbrev(tcp))
		max_count = (max_strlen < count) ? max_strlen : count;
	else
		max_count = count;

	if (!max_count) {
		tprintf("%#lx, %lu", addr, count);
		return;
	}

	for(i = 0; i < max_count; ++i) {
		struct sembuf sb;
		if (i)
			tprintf(", ");
		if (umove(tcp, addr + i * sizeof(struct sembuf), &sb) < 0) {
			if (i) {
				tprintf("{???}");
				break;
			} else {
				tprintf("%#lx, %lu", addr, count);
				return;
			}
		} else {
			if (!i)
				tprintf("{");
			tprintf("{%u, %d, ", sb.sem_num, sb.sem_op);
			printflags(semop_flags, sb.sem_flg, "SEM_???");
			tprintf("}");
		}
	}

	if (i < max_count || max_count < count)
		tprintf(", ...");

	tprintf("}, %lu", count);
}

int sys_semop(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
		if (indirect_ipccall(tcp)) {
			tprint_sembuf(tcp, tcp->u_arg[3], tcp->u_arg[1]);
		} else {
			tprint_sembuf(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		}
	}
	return 0;
}

#ifdef LINUX
int sys_semtimedop(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
		if (indirect_ipccall(tcp)) {
			tprint_sembuf(tcp, tcp->u_arg[3], tcp->u_arg[1]);
			tprintf(", ");
			printtv(tcp, tcp->u_arg[5]);
		} else {
			tprint_sembuf(tcp, tcp->u_arg[1], tcp->u_arg[2]);
			tprintf(", ");
			printtv(tcp, tcp->u_arg[3]);
		}
	}
	return 0;
}
#endif

int sys_semget(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		if (tcp->u_arg[0])
			tprintf("%#lx", tcp->u_arg[0]);
		else
			tprintf("IPC_PRIVATE");
		tprintf(", %lu", tcp->u_arg[1]);
		tprintf(", ");
		if (printflags(resource_flags, tcp->u_arg[2] & ~0777, NULL) != 0)
			tprintf("|");
		tprintf("%#lo", tcp->u_arg[2] & 0777);
	}
	return 0;
}

int sys_semctl(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu", tcp->u_arg[0]);
		tprintf(", %lu, ", tcp->u_arg[1]);
		PRINTCTL(semctl_flags, tcp->u_arg[2], "SEM_???");
		tprintf(", %#lx", tcp->u_arg[3]);
	}
	return 0;
}

int sys_shmget(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		if (tcp->u_arg[0])
			tprintf("%#lx", tcp->u_arg[0]);
		else
			tprintf("IPC_PRIVATE");
		tprintf(", %lu", tcp->u_arg[1]);
		tprintf(", ");
		if (printflags(shm_resource_flags, tcp->u_arg[2] & ~0777, NULL) != 0)
			tprintf("|");
		tprintf("%#lo", tcp->u_arg[2] & 0777);
	}
	return 0;
}

int sys_shmctl(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
		PRINTCTL(shmctl_flags, tcp->u_arg[1], "SHM_???");
		if (indirect_ipccall(tcp)) {
			tprintf(", %#lx", tcp->u_arg[3]);
		} else {
			tprintf(", %#lx", tcp->u_arg[2]);
		}
	}
	return 0;
}

int sys_shmat(tcp)
struct tcb *tcp;
{
#ifdef LINUX
	unsigned long raddr;
#endif /* LINUX */

	if (exiting(tcp)) {
		tprintf("%lu", tcp->u_arg[0]);
		if (indirect_ipccall(tcp)) {
			tprintf(", %#lx", tcp->u_arg[3]);
			tprintf(", ");
			printflags(shm_flags, tcp->u_arg[1], "SHM_???");
		} else {
			tprintf(", %#lx", tcp->u_arg[1]);
			tprintf(", ");
			printflags(shm_flags, tcp->u_arg[2], "SHM_???");
		}
		if (syserror(tcp))
			return 0;
/* HPPA does not use an IPC multiplexer on Linux.  */
#if defined(LINUX) && !defined(HPPA)
		if (umove(tcp, tcp->u_arg[2], &raddr) < 0)
			return RVAL_NONE;
		tcp->u_rval = raddr;
#endif /* LINUX */
		return RVAL_HEX;
	}
	return 0;
}

int sys_shmdt(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		if (indirect_ipccall(tcp)) {
			tprintf("%#lx", tcp->u_arg[3]);
		} else {
			tprintf("%#lx", tcp->u_arg[0]);
		}
	}
	return 0;
}

#endif /* defined(LINUX) || defined(SUNOS4) || defined(FREEBSD) */

#ifdef LINUX
int
sys_mq_open(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		/* flags */
		tprint_open_modes(tcp->u_arg[1]);
		if (tcp->u_arg[1] & O_CREAT) {
# ifndef HAVE_MQUEUE_H
			tprintf(", %lx", tcp->u_arg[2]);
# else
			struct mq_attr attr;
			/* mode */
			tprintf(", %#lo, ", tcp->u_arg[2]);
			if (umove(tcp, tcp->u_arg[3], &attr) < 0)
				tprintf("{ ??? }");
			else
				tprintf("{mq_maxmsg=%ld, mq_msgsize=%ld}",
					attr.mq_maxmsg, attr.mq_msgsize);
# endif
		}
	}
	return 0;
}

int
sys_mq_timedsend(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, %ld, ", tcp->u_arg[2], tcp->u_arg[3]);
		printtv(tcp, tcp->u_arg[4]);
	}
	return 0;
}

int
sys_mq_timedreceive(struct tcb *tcp)
{
	if (entering(tcp))
		tprintf("%ld, ", tcp->u_arg[0]);
	else {
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, %ld, ", tcp->u_arg[2], tcp->u_arg[3]);
		printtv(tcp, tcp->u_arg[4]);
	}
	return 0;
}

int
sys_mq_notify(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printsigevent(tcp, tcp->u_arg[1]);
	}
	return 0;
}

static void
printmqattr(struct tcb *tcp, long addr)
{
	if (addr == 0)
		tprintf("NULL");
	else {
# ifndef HAVE_MQUEUE_H
		tprintf("%#lx", addr);
# else
		struct mq_attr attr;
		if (umove(tcp, addr, &attr) < 0) {
			tprintf("{...}");
			return;
		}
		tprintf("{mq_flags=");
		tprint_open_modes(attr.mq_flags);
		tprintf(", mq_maxmsg=%ld, mq_msgsize=%ld, mq_curmsg=%ld}",
			attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);
# endif
	}
}

int
sys_mq_getsetattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printmqattr(tcp, tcp->u_arg[1]);
		tprintf(", ");
	} else
		printmqattr(tcp, tcp->u_arg[2]);
	return 0;
}
#endif
