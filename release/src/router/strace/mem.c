/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2000 PocketPenguins Inc.  Linux for Hitachi SuperH
 *                    port by Greg Banks <gbanks@pocketpenguins.com>
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

#ifdef LINUX
#include <asm/mman.h>
#endif
#include <sys/mman.h>

#if defined(LINUX) && defined(I386)
#include <asm/ldt.h>
# ifdef HAVE_STRUCT_USER_DESC
#  define modify_ldt_ldt_s user_desc
# endif
#endif
#if defined(LINUX) && defined(SH64)
#include <asm/page.h>	    /* for PAGE_SHIFT */
#endif

#ifdef HAVE_LONG_LONG_OFF_T
/*
 * Ugly hacks for systems that have a long long off_t
 */
#define sys_mmap64	sys_mmap
#endif

int
sys_brk(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx", tcp->u_arg[0]);
	}
#ifdef LINUX
	return RVAL_HEX;
#else
	return 0;
#endif
}

#if defined(FREEBSD) || defined(SUNOS4)
int
sys_sbrk(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%lu", tcp->u_arg[0]);
	}
	return RVAL_HEX;
}
#endif /* FREEBSD || SUNOS4 */

static const struct xlat mmap_prot[] = {
	{ PROT_NONE,	"PROT_NONE",	},
	{ PROT_READ,	"PROT_READ"	},
	{ PROT_WRITE,	"PROT_WRITE"	},
	{ PROT_EXEC,	"PROT_EXEC"	},
#ifdef PROT_SEM
	{ PROT_SEM,	"PROT_SEM"	},
#endif
#ifdef PROT_GROWSDOWN
	{ PROT_GROWSDOWN,"PROT_GROWSDOWN"},
#endif
#ifdef PROT_GROWSUP
	{ PROT_GROWSUP, "PROT_GROWSUP"	},
#endif
#ifdef PROT_SAO
	{ PROT_SAO,	"PROT_SAO"	},
#endif
	{ 0,		NULL		},
};

static const struct xlat mmap_flags[] = {
	{ MAP_SHARED,	"MAP_SHARED"	},
	{ MAP_PRIVATE,	"MAP_PRIVATE"	},
	{ MAP_FIXED,	"MAP_FIXED"	},
#ifdef MAP_ANONYMOUS
	{ MAP_ANONYMOUS,"MAP_ANONYMOUS"	},
#endif
#ifdef MAP_32BIT
	{ MAP_32BIT,	"MAP_32BIT"	},
#endif
#ifdef MAP_RENAME
	{ MAP_RENAME,	"MAP_RENAME"	},
#endif
#ifdef MAP_NORESERVE
	{ MAP_NORESERVE,"MAP_NORESERVE"	},
#endif
#ifdef MAP_POPULATE
	{ MAP_POPULATE, "MAP_POPULATE" },
#endif
#ifdef MAP_NONBLOCK
	{ MAP_NONBLOCK, "MAP_NONBLOCK" },
#endif
	/*
	 * XXX - this was introduced in SunOS 4.x to distinguish between
	 * the old pre-4.x "mmap()", which:
	 *
	 *	only let you map devices with an "mmap" routine (e.g.,
	 *	frame buffers) in;
	 *
	 *	required you to specify the mapping address;
	 *
	 *	returned 0 on success and -1 on failure;
	 *
	 * memory and which, and the 4.x "mmap()" which:
	 *
	 *	can map plain files;
	 *
	 *	can be asked to pick where to map the file;
	 *
	 *	returns the address where it mapped the file on success
	 *	and -1 on failure.
	 *
	 * It's not actually used in source code that calls "mmap()"; the
	 * "mmap()" routine adds it for you.
	 *
	 * It'd be nice to come up with some way of eliminating it from
	 * the flags, e.g. reporting calls *without* it as "old_mmap()"
	 * and calls with it as "mmap()".
	 */
#ifdef _MAP_NEW
	{ _MAP_NEW,	"_MAP_NEW"	},
#endif
#ifdef MAP_GROWSDOWN
	{ MAP_GROWSDOWN,"MAP_GROWSDOWN"	},
#endif
#ifdef MAP_DENYWRITE
	{ MAP_DENYWRITE,"MAP_DENYWRITE"	},
#endif
#ifdef MAP_EXECUTABLE
	{ MAP_EXECUTABLE,"MAP_EXECUTABLE"},
#endif
#ifdef MAP_INHERIT
	{ MAP_INHERIT,"MAP_INHERIT"	},
#endif
#ifdef MAP_FILE
	{ MAP_FILE,"MAP_FILE"},
#endif
#ifdef MAP_LOCKED
	{ MAP_LOCKED,"MAP_LOCKED"},
#endif
	/* FreeBSD ones */
#ifdef MAP_ANON
	{ MAP_ANON,		"MAP_ANON"	},
#endif
#ifdef MAP_HASSEMAPHORE
	{ MAP_HASSEMAPHORE,	"MAP_HASSEMAPHORE"	},
#endif
#ifdef MAP_STACK
	{ MAP_STACK,		"MAP_STACK"	},
#endif
#ifdef MAP_NOSYNC
	{ MAP_NOSYNC,		"MAP_NOSYNC"	},
#endif
#ifdef MAP_NOCORE
	{ MAP_NOCORE,		"MAP_NOCORE"	},
#endif
#ifdef TILE
	{ MAP_CACHE_NO_LOCAL, "MAP_CACHE_NO_LOCAL" },
	{ MAP_CACHE_NO_L2, "MAP_CACHE_NO_L2" },
	{ MAP_CACHE_NO_L1, "MAP_CACHE_NO_L1" },
#endif
	{ 0,		NULL		},
};

#ifdef TILE
static
int
addtileflags(flags)
long flags;
{
	long home = flags & _MAP_CACHE_MKHOME(_MAP_CACHE_HOME_MASK);
	flags &= ~_MAP_CACHE_MKHOME(_MAP_CACHE_HOME_MASK);

	if (flags & _MAP_CACHE_INCOHERENT) {
		flags &= ~_MAP_CACHE_INCOHERENT;
		if (home == MAP_CACHE_HOME_NONE) {
			tprintf("|MAP_CACHE_INCOHERENT");
			return flags;
		}
		tprintf("|_MAP_CACHE_INCOHERENT");
	}

	switch (home) {
	case 0:	break;
	case MAP_CACHE_HOME_HERE: tprintf("|MAP_CACHE_HOME_HERE"); break;
	case MAP_CACHE_HOME_NONE: tprintf("|MAP_CACHE_HOME_NONE"); break;
	case MAP_CACHE_HOME_SINGLE: tprintf("|MAP_CACHE_HOME_SINGLE"); break;
	case MAP_CACHE_HOME_TASK: tprintf("|MAP_CACHE_HOME_TASK"); break;
	case MAP_CACHE_HOME_HASH: tprintf("|MAP_CACHE_HOME_HASH"); break;
	default:
		tprintf("|MAP_CACHE_HOME(%d)",
			(home >> _MAP_CACHE_HOME_SHIFT) );
		break;
	}

	return flags;
}
#endif

#if !HAVE_LONG_LONG_OFF_T
static int
print_mmap(struct tcb *tcp, long *u_arg, long long offset)
{
	if (entering(tcp)) {
		/* addr */
		if (!u_arg[0])
			tprintf("NULL, ");
		else
			tprintf("%#lx, ", u_arg[0]);
		/* len */
		tprintf("%lu, ", u_arg[1]);
		/* prot */
		printflags(mmap_prot, u_arg[2], "PROT_???");
		tprintf(", ");
		/* flags */
#ifdef MAP_TYPE
		printxval(mmap_flags, u_arg[3] & MAP_TYPE, "MAP_???");
#ifdef TILE
		addflags(mmap_flags, addtileflags(u_arg[3] & ~MAP_TYPE));
#else
		addflags(mmap_flags, u_arg[3] & ~MAP_TYPE);
#endif
#else
		printflags(mmap_flags, u_arg[3], "MAP_???");
#endif
		/* fd */
		tprintf(", ");
		printfd(tcp, u_arg[4]);
		/* offset */
		tprintf(", %#llx", offset);
	}
	return RVAL_HEX;
}

#ifdef LINUX
int sys_old_mmap(tcp)
struct tcb *tcp;
{
	long u_arg[6];

#if	defined(IA64)
	int i, v;
	/*
	 *  IA64 processes never call this routine, they only use the
	 *  new `sys_mmap' interface.  This code converts the integer
	 *  arguments that the IA32 process pushed onto the stack into
	 *  longs.
	 *
	 *  Note that addresses with bit 31 set will be sign extended.
	 *  Fortunately, those addresses are not currently being generated
	 *  for IA32 processes so it's not a problem.
	 */
	for (i = 0; i < 6; i++)
		if (umove(tcp, tcp->u_arg[0] + (i * sizeof(int)), &v) == -1)
			return 0;
		else
			u_arg[i] = v;
#elif defined(SH) || defined(SH64)
	/* SH has always passed the args in registers */
	int i;
	for (i=0; i<6; i++)
		u_arg[i] = tcp->u_arg[i];
#else
# if defined(X86_64)
	if (current_personality == 1) {
		int i;
		for (i = 0; i < 6; ++i) {
			unsigned int val;
			if (umove(tcp, tcp->u_arg[0] + i * 4, &val) == -1)
				return 0;
			u_arg[i] = val;
		}
	}
	else
# endif
	if (umoven(tcp, tcp->u_arg[0], sizeof u_arg, (char *) u_arg) == -1)
		return 0;
#endif	// defined(IA64)
	return print_mmap(tcp, u_arg, u_arg[5]);
}
#endif

int
sys_mmap(tcp)
struct tcb *tcp;
{
	long long offset = tcp->u_arg[5];

#if defined(LINUX) && defined(SH64)
	/*
	 * Old mmap differs from new mmap in specifying the
	 * offset in units of bytes rather than pages.  We
	 * pretend it's in byte units so the user only ever
	 * sees bytes in the printout.
	 */
	offset <<= PAGE_SHIFT;
#endif
#if defined(LINUX_MIPSN32)
	offset = tcp->ext_arg[5];
#endif
	return print_mmap(tcp, tcp->u_arg, offset);
}
#endif /* !HAVE_LONG_LONG_OFF_T */

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_mmap64(struct tcb *tcp)
{
#ifdef linux
#ifdef ALPHA
	long *u_arg = tcp->u_arg;
#else /* !ALPHA */
	long u_arg[7];
#endif /* !ALPHA */
#else /* !linux */
	long *u_arg = tcp->u_arg;
#endif /* !linux */

	if (entering(tcp)) {
#ifdef linux
#ifndef ALPHA
		if (umoven(tcp, tcp->u_arg[0], sizeof u_arg,
				(char *) u_arg) == -1)
			return 0;
#endif /* ALPHA */
#endif /* linux */

		/* addr */
		tprintf("%#lx, ", u_arg[0]);
		/* len */
		tprintf("%lu, ", u_arg[1]);
		/* prot */
		printflags(mmap_prot, u_arg[2], "PROT_???");
		tprintf(", ");
		/* flags */
#ifdef MAP_TYPE
		printxval(mmap_flags, u_arg[3] & MAP_TYPE, "MAP_???");
		addflags(mmap_flags, u_arg[3] & ~MAP_TYPE);
#else
		printflags(mmap_flags, u_arg[3], "MAP_???");
#endif
		/* fd */
		tprintf(", ");
		printfd(tcp, tcp->u_arg[4]);
		/* offset */
		printllval(tcp, ", %#llx", 5);
	}
	return RVAL_HEX;
}
#endif


int
sys_munmap(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu",
			tcp->u_arg[0], tcp->u_arg[1]);
	}
	return 0;
}

int
sys_mprotect(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu, ",
			tcp->u_arg[0], tcp->u_arg[1]);
		printflags(mmap_prot, tcp->u_arg[2], "PROT_???");
	}
	return 0;
}

#ifdef LINUX

static const struct xlat mremap_flags[] = {
	{ MREMAP_MAYMOVE,	"MREMAP_MAYMOVE"	},
#ifdef MREMAP_FIXED
	{ MREMAP_FIXED,		"MREMAP_FIXED"		},
#endif
	{ 0,			NULL			}
};

int
sys_mremap(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu, %lu, ", tcp->u_arg[0], tcp->u_arg[1],
			tcp->u_arg[2]);
		printflags(mremap_flags, tcp->u_arg[3], "MREMAP_???");
#ifdef MREMAP_FIXED
		if ((tcp->u_arg[3] & (MREMAP_MAYMOVE | MREMAP_FIXED)) ==
		    (MREMAP_MAYMOVE | MREMAP_FIXED))
			tprintf(", %#lx", tcp->u_arg[4]);
#endif
	}
	return RVAL_HEX;
}

static const struct xlat madvise_cmds[] = {
#ifdef MADV_NORMAL
	{ MADV_NORMAL,		"MADV_NORMAL" },
#endif
#ifdef MADV_RANDOM
	{ MADV_RANDOM,		"MADV_RANDOM" },
#endif
#ifdef MADV_SEQUENTIAL
	{ MADV_SEQUENTIAL,	"MADV_SEQUENTIAL" },
#endif
#ifdef MADV_WILLNEED
	{ MADV_WILLNEED,	"MADV_WILLNEED" },
#endif
#ifdef MADV_DONTNEED
	{ MADV_DONTNEED,	"MADV_DONTNEED" },
#endif
	{ 0,			NULL },
};


int
sys_madvise(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
		printxval(madvise_cmds, tcp->u_arg[2], "MADV_???");
	}
	return 0;
}


static const struct xlat mlockall_flags[] = {
#ifdef MCL_CURRENT
	{ MCL_CURRENT,	"MCL_CURRENT" },
#endif
#ifdef MCL_FUTURE
	{ MCL_FUTURE,	"MCL_FUTURE" },
#endif
	{ 0,		NULL}
};

int
sys_mlockall(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printflags(mlockall_flags, tcp->u_arg[0], "MCL_???");
	}
	return 0;
}


#endif /* LINUX */

#ifdef MS_ASYNC

static const struct xlat mctl_sync[] = {
#ifdef MS_SYNC
	{ MS_SYNC,	"MS_SYNC"	},
#endif
	{ MS_ASYNC,	"MS_ASYNC"	},
	{ MS_INVALIDATE,"MS_INVALIDATE"	},
	{ 0,		NULL		},
};

int
sys_msync(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		/* addr */
		tprintf("%#lx", tcp->u_arg[0]);
		/* len */
		tprintf(", %lu, ", tcp->u_arg[1]);
		/* flags */
		printflags(mctl_sync, tcp->u_arg[2], "MS_???");
	}
	return 0;
}

#endif /* MS_ASYNC */

#ifdef MC_SYNC

static const struct xlat mctl_funcs[] = {
	{ MC_LOCK,	"MC_LOCK"	},
	{ MC_LOCKAS,	"MC_LOCKAS"	},
	{ MC_SYNC,	"MC_SYNC"	},
	{ MC_UNLOCK,	"MC_UNLOCK"	},
	{ MC_UNLOCKAS,	"MC_UNLOCKAS"	},
	{ 0,		NULL		},
};

static const struct xlat mctl_lockas[] = {
	{ MCL_CURRENT,	"MCL_CURRENT"	},
	{ MCL_FUTURE,	"MCL_FUTURE"	},
	{ 0,		NULL		},
};

int
sys_mctl(tcp)
struct tcb *tcp;
{
	int arg, function;

	if (entering(tcp)) {
		/* addr */
		tprintf("%#lx", tcp->u_arg[0]);
		/* len */
		tprintf(", %lu, ", tcp->u_arg[1]);
		/* function */
		function = tcp->u_arg[2];
		printflags(mctl_funcs, function, "MC_???");
		/* arg */
		arg = tcp->u_arg[3];
		tprintf(", ");
		switch (function) {
		case MC_SYNC:
			printflags(mctl_sync, arg, "MS_???");
			break;
		case MC_LOCKAS:
			printflags(mctl_lockas, arg, "MCL_???");
			break;
		default:
			tprintf("%#x", arg);
			break;
		}
	}
	return 0;
}

#endif /* MC_SYNC */

int
sys_mincore(tcp)
struct tcb *tcp;
{
	unsigned long i, len;
	char *vec = NULL;

	if (entering(tcp)) {
		tprintf("%#lx, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
	} else {
		len = tcp->u_arg[1];
		if (syserror(tcp) || tcp->u_arg[2] == 0 ||
			(vec = malloc(len)) == NULL ||
			umoven(tcp, tcp->u_arg[2], len, vec) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else {
			tprintf("[");
			for (i = 0; i < len; i++) {
				if (abbrev(tcp) && i >= max_strlen) {
					tprintf("...");
					break;
				}
				tprintf((vec[i] & 1) ? "1" : "0");
			}
			tprintf("]");
		}
		if (vec)
			free(vec);
	}
	return 0;
}

#if defined(ALPHA) || defined(FREEBSD) || defined(IA64) || defined(SUNOS4) || defined(SVR4) || defined(SPARC) || defined(SPARC64)
int
sys_getpagesize(tcp)
struct tcb *tcp;
{
	if (exiting(tcp))
		return RVAL_HEX;
	return 0;
}
#endif /* ALPHA || FREEBSD || IA64 || SUNOS4 || SVR4 */

#if defined(LINUX) && defined(__i386__)
void
print_ldt_entry(struct modify_ldt_ldt_s *ldt_entry)
{
	tprintf("base_addr:%#08lx, "
		"limit:%d, "
		"seg_32bit:%d, "
		"contents:%d, "
		"read_exec_only:%d, "
		"limit_in_pages:%d, "
		"seg_not_present:%d, "
		"useable:%d}",
		(long) ldt_entry->base_addr,
		ldt_entry->limit,
		ldt_entry->seg_32bit,
		ldt_entry->contents,
		ldt_entry->read_exec_only,
		ldt_entry->limit_in_pages,
		ldt_entry->seg_not_present,
		ldt_entry->useable);
}

int
sys_modify_ldt(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		struct modify_ldt_ldt_s copy;
		tprintf("%ld", tcp->u_arg[0]);
		if (tcp->u_arg[1] == 0
				|| tcp->u_arg[2] != sizeof (struct modify_ldt_ldt_s)
				|| umove(tcp, tcp->u_arg[1], &copy) == -1)
			tprintf(", %lx", tcp->u_arg[1]);
		else {
			tprintf(", {entry_number:%d, ", copy.entry_number);
			if (!verbose(tcp))
				tprintf("...}");
			else {
				print_ldt_entry(&copy);
			}
		}
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

int
sys_set_thread_area(tcp)
struct tcb *tcp;
{
	struct modify_ldt_ldt_s copy;
	if (entering(tcp)) {
		if (umove(tcp, tcp->u_arg[0], &copy) != -1) {
			if (copy.entry_number == -1)
				tprintf("{entry_number:%d -> ",
					copy.entry_number);
			else
				tprintf("{entry_number:");
		}
	} else {
		if (umove(tcp, tcp->u_arg[0], &copy) != -1) {
			tprintf("%d, ", copy.entry_number);
			if (!verbose(tcp))
				tprintf("...}");
			else {
				print_ldt_entry(&copy);
			}
		} else {
			tprintf("%lx", tcp->u_arg[0]);
		}
	}
	return 0;

}

int
sys_get_thread_area(tcp)
struct tcb *tcp;
{
	struct modify_ldt_ldt_s copy;
	if (exiting(tcp)) {
		if (umove(tcp, tcp->u_arg[0], &copy) != -1) {
			tprintf("{entry_number:%d, ", copy.entry_number);
			if (!verbose(tcp))
				tprintf("...}");
			else {
				print_ldt_entry(&copy);
			}
		} else {
			tprintf("%lx", tcp->u_arg[0]);
		}
	}
	return 0;

}
#endif /* LINUX && __i386__ */

#if defined(LINUX) && defined(M68K)

int
sys_set_thread_area(tcp)
struct tcb *tcp;
{
	if (entering(tcp))
		tprintf("%#lx", tcp->u_arg[0]);
	return 0;

}

int
sys_get_thread_area(tcp)
struct tcb *tcp;
{
	return RVAL_HEX;
}
#endif

#if defined(LINUX)
int
sys_remap_file_pages(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
		printflags(mmap_prot, tcp->u_arg[2], "PROT_???");
		tprintf(", %lu, ", tcp->u_arg[3]);
#ifdef MAP_TYPE
		printxval(mmap_flags, tcp->u_arg[4] & MAP_TYPE, "MAP_???");
		addflags(mmap_flags, tcp->u_arg[4] & ~MAP_TYPE);
#else
		printflags(mmap_flags, tcp->u_arg[4], "MAP_???");
#endif
	}
	return 0;
}


#define MPOL_DEFAULT    0
#define MPOL_PREFERRED  1
#define MPOL_BIND       2
#define MPOL_INTERLEAVE 3

#define MPOL_F_NODE     (1<<0)
#define MPOL_F_ADDR     (1<<1)

#define MPOL_MF_STRICT  (1<<0)
#define MPOL_MF_MOVE	(1<<1)
#define MPOL_MF_MOVE_ALL (1<<2)


static const struct xlat policies[] = {
	{ MPOL_DEFAULT,		"MPOL_DEFAULT"		},
	{ MPOL_PREFERRED,	"MPOL_PREFERRED"	},
	{ MPOL_BIND,		"MPOL_BIND"		},
	{ MPOL_INTERLEAVE,	"MPOL_INTERLEAVE"	},
	{ 0,			NULL			}
};

static const struct xlat mbindflags[] = {
	{ MPOL_MF_STRICT,	"MPOL_MF_STRICT"	},
	{ MPOL_MF_MOVE,		"MPOL_MF_MOVE"		},
	{ MPOL_MF_MOVE_ALL,	"MPOL_MF_MOVE_ALL"	},
	{ 0,			NULL			}
};

static const struct xlat mempolicyflags[] = {
	{ MPOL_F_NODE,		"MPOL_F_NODE"		},
	{ MPOL_F_ADDR,		"MPOL_F_ADDR"		},
	{ 0,			NULL			}
};

static const struct xlat move_pages_flags[] = {
	{ MPOL_MF_MOVE,		"MPOL_MF_MOVE"		},
	{ MPOL_MF_MOVE_ALL,	"MPOL_MF_MOVE_ALL"	},
	{ 0,			NULL			}
};


static void
get_nodes(tcp, ptr, maxnodes, err)
struct tcb *tcp;
unsigned long ptr;
unsigned long maxnodes;
int err;
{
	unsigned long nlongs, size, end;

	nlongs = (maxnodes + 8 * sizeof(long) - 1) / (8 * sizeof(long));
	size = nlongs * sizeof(long);
	end = ptr + size;
	if (nlongs == 0 || ((err || verbose(tcp)) && (size * 8 == maxnodes)
			    && (end > ptr))) {
		unsigned long n, cur, abbrev_end;
		int failed = 0;

		if (abbrev(tcp)) {
			abbrev_end = ptr + max_strlen * sizeof(long);
			if (abbrev_end < ptr)
				abbrev_end = end;
		} else {
			abbrev_end = end;
		}
		tprintf(", {");
		for (cur = ptr; cur < end; cur += sizeof(long)) {
			if (cur > ptr)
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umoven(tcp, cur, sizeof(n), (char *) &n) < 0) {
				tprintf("?");
				failed = 1;
				break;
			}
			tprintf("%#0*lx", (int) sizeof(long) * 2 + 2, n);
		}
		tprintf("}");
		if (failed)
			tprintf(" %#lx", ptr);
	} else
		tprintf(", %#lx", ptr);
	tprintf(", %lu", maxnodes);
}

int
sys_mbind(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%#lx, %lu, ", tcp->u_arg[0], tcp->u_arg[1]);
		printxval(policies, tcp->u_arg[2], "MPOL_???");
		get_nodes(tcp, tcp->u_arg[3], tcp->u_arg[4], 0);
		tprintf(", ");
		printflags(mbindflags, tcp->u_arg[5], "MPOL_???");
	}
	return 0;
}

int
sys_set_mempolicy(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printxval(policies, tcp->u_arg[0], "MPOL_???");
		get_nodes(tcp, tcp->u_arg[1], tcp->u_arg[2], 0);
	}
	return 0;
}

int
sys_get_mempolicy(tcp)
struct tcb *tcp;
{
	if (exiting(tcp)) {
		int pol;
		if (tcp->u_arg[0] == 0)
			tprintf("NULL");
		else if (syserror(tcp) || umove(tcp, tcp->u_arg[0], &pol) < 0)
			tprintf("%#lx", tcp->u_arg[0]);
		else
			printxval(policies, pol, "MPOL_???");
		get_nodes(tcp, tcp->u_arg[1], tcp->u_arg[2], syserror(tcp));
		tprintf(", %#lx, ", tcp->u_arg[3]);
		printflags(mempolicyflags, tcp->u_arg[4], "MPOL_???");
	}
	return 0;
}

int
sys_move_pages(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		unsigned long npages = tcp->u_arg[1];
		tprintf("%ld, %lu, ", tcp->u_arg[0], npages);
		if (tcp->u_arg[2] == 0)
			tprintf("NULL, ");
		else {
			int i;
			long puser = tcp->u_arg[2];
			tprintf("{");
			for (i = 0; i < npages; ++i) {
				void *p;
				if (i > 0)
					tprintf(", ");
				if (umove(tcp, puser, &p) < 0) {
					tprintf("???");
					break;
				}
				tprintf("%p", p);
				puser += sizeof (void *);
			}
			tprintf("}, ");
		}
		if (tcp->u_arg[3] == 0)
			tprintf("NULL, ");
		else {
			int i;
			long nodeuser = tcp->u_arg[3];
			tprintf("{");
			for (i = 0; i < npages; ++i) {
				int node;
				if (i > 0)
					tprintf(", ");
				if (umove(tcp, nodeuser, &node) < 0) {
					tprintf("???");
					break;
				}
				tprintf("%#x", node);
				nodeuser += sizeof (int);
			}
			tprintf("}, ");
		}
	}
	if (exiting(tcp)) {
		unsigned long npages = tcp->u_arg[1];
		if (tcp->u_arg[4] == 0)
			tprintf("NULL, ");
		else {
			int i;
			long statususer = tcp->u_arg[4];
			tprintf("{");
			for (i = 0; i < npages; ++i) {
				int status;
				if (i > 0)
					tprintf(", ");
				if (umove(tcp, statususer, &status) < 0) {
					tprintf("???");
					break;
				}
				tprintf("%#x", status);
				statususer += sizeof (int);
			}
			tprintf("}, ");
		}
		printflags(move_pages_flags, tcp->u_arg[5], "MPOL_???");
	}
	return 0;
}
#endif

#if defined(LINUX) && defined(POWERPC)
int
sys_subpage_prot(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		unsigned long cur, end, abbrev_end, entries;
		unsigned int entry;

		tprintf("%#lx, %#lx, ", tcp->u_arg[0], tcp->u_arg[1]);
		entries = tcp->u_arg[1] >> 16;
		if (!entries || !tcp->u_arg[2]) {
			tprintf("{}");
			return 0;
		}
		cur = tcp->u_arg[2];
		end = cur + (sizeof(int) * entries);
		if (!verbose(tcp) || end < tcp->u_arg[2]) {
			tprintf("%#lx", tcp->u_arg[2]);
			return 0;
		}
		if (abbrev(tcp)) {
			abbrev_end = cur + (sizeof(int) * max_strlen);
			if (abbrev_end > end)
				abbrev_end = end;
		}
		else
			abbrev_end = end;
		tprintf("{");
		for (; cur < end; cur += sizeof(int)) {
			if (cur > tcp->u_arg[2])
				tprintf(", ");
			if (cur >= abbrev_end) {
				tprintf("...");
				break;
			}
			if (umove(tcp, cur, &entry) < 0) {
				tprintf("??? [%#lx]", cur);
				break;
			}
			else
				tprintf("%#08x", entry);
		}
		tprintf("}");
	}

	return 0;
}
#endif
