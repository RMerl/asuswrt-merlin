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

#include <signal.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/param.h>
#include <fcntl.h>
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef SUNOS4
#include <machine/reg.h>
#include <a.out.h>
#include <link.h>
#endif /* SUNOS4 */

#if defined(linux) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 1))
#include <linux/ptrace.h>
#endif

#if defined(LINUX) && defined(IA64)
# include <asm/ptrace_offsets.h>
# include <asm/rse.h>
#endif

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
# define PTRACE_PEEKUSR PTRACE_PEEKUSER
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

#ifdef SUNOS4_KERNEL_ARCH_KLUDGE
#include <sys/utsname.h>
#endif /* SUNOS4_KERNEL_ARCH_KLUDGE */

#if defined(LINUXSPARC) && defined (SPARC64)
# undef PTRACE_GETREGS
# define PTRACE_GETREGS PTRACE_GETREGS64
# undef PTRACE_SETREGS
# define PTRACE_SETREGS PTRACE_SETREGS64
#endif

/* macros */
#ifndef MAX
#define MAX(a,b)		(((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)		(((a) < (b)) ? (a) : (b))
#endif

int
tv_nz(a)
struct timeval *a;
{
	return a->tv_sec || a->tv_usec;
}

int
tv_cmp(a, b)
struct timeval *a, *b;
{
	if (a->tv_sec < b->tv_sec
	    || (a->tv_sec == b->tv_sec && a->tv_usec < b->tv_usec))
		return -1;
	if (a->tv_sec > b->tv_sec
	    || (a->tv_sec == b->tv_sec && a->tv_usec > b->tv_usec))
		return 1;
	return 0;
}

double
tv_float(tv)
struct timeval *tv;
{
	return tv->tv_sec + tv->tv_usec/1000000.0;
}

void
tv_add(tv, a, b)
struct timeval *tv, *a, *b;
{
	tv->tv_sec = a->tv_sec + b->tv_sec;
	tv->tv_usec = a->tv_usec + b->tv_usec;
	if (tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
}

void
tv_sub(tv, a, b)
struct timeval *tv, *a, *b;
{
	tv->tv_sec = a->tv_sec - b->tv_sec;
	tv->tv_usec = a->tv_usec - b->tv_usec;
	if (((long) tv->tv_usec) < 0) {
		tv->tv_sec--;
		tv->tv_usec += 1000000;
	}
}

void
tv_div(tv, a, n)
struct timeval *tv, *a;
int n;
{
	tv->tv_usec = (a->tv_sec % n * 1000000 + a->tv_usec + n / 2) / n;
	tv->tv_sec = a->tv_sec / n + tv->tv_usec / 1000000;
	tv->tv_usec %= 1000000;
}

void
tv_mul(tv, a, n)
struct timeval *tv, *a;
int n;
{
	tv->tv_usec = a->tv_usec * n;
	tv->tv_sec = a->tv_sec * n + tv->tv_usec / 1000000;
	tv->tv_usec %= 1000000;
}

const char *
xlookup(const struct xlat *xlat, int val)
{
	for (; xlat->str != NULL; xlat++)
		if (xlat->val == val)
			return xlat->str;
	return NULL;
}

/*
 * Generic ptrace wrapper which tracks ESRCH errors
 * by setting tcp->ptrace_errno to ESRCH.
 *
 * We assume that ESRCH indicates likely process death (SIGKILL?),
 * modulo bugs where process somehow ended up not stopped.
 * Unfortunately kernel uses ESRCH for that case too. Oh well.
 *
 * Currently used by upeek() only.
 * TODO: use this in all other ptrace() calls while decoding.
 */
long
do_ptrace(int request, struct tcb *tcp, void *addr, void *data)
{
	long l;

	errno = 0;
	l = ptrace(request, tcp->pid, addr, (long) data);
	/* Non-ESRCH errors might be our invalid reg/mem accesses,
	 * we do not record them. */
	if (errno == ESRCH)
		tcp->ptrace_errno = ESRCH;
	return l;
}

/*
 * Used when we want to unblock stopped traced process.
 * Should be only used with PTRACE_CONT, PTRACE_DETACH and PTRACE_SYSCALL.
 * Returns 0 on success or if error was ESRCH
 * (presumably process was killed while we talk to it).
 * Otherwise prints error message and returns -1.
 */
int
ptrace_restart(int op, struct tcb *tcp, int sig)
{
	int err;
	const char *msg;

	errno = 0;
	ptrace(op, tcp->pid, (void *) 1, (long) sig);
	err = errno;
	if (!err || err == ESRCH)
		return 0;

	tcp->ptrace_errno = err;
	msg = "SYSCALL";
	if (op == PTRACE_CONT)
		msg = "CONT";
	if (op == PTRACE_DETACH)
		msg = "DETACH";
	fprintf(stderr, "strace: ptrace(PTRACE_%s,1,%d): %s\n",
			msg, sig, strerror(err));
	return -1;
}

/*
 * Print entry in struct xlat table, if there.
 */
void
printxval(const struct xlat *xlat, int val, const char *dflt)
{
	const char *str = xlookup(xlat, val);

	if (str)
		tprintf("%s", str);
	else
		tprintf("%#x /* %s */", val, dflt);
}

#if HAVE_LONG_LONG
/*
 * Print 64bit argument at position llarg and return the index of the next
 * argument.
 */
int
printllval(struct tcb *tcp, const char *format, int llarg)
{
# if defined(FREEBSD) \
     || (defined(LINUX) && defined(POWERPC) && !defined(POWERPC64)) \
     || defined (LINUX_MIPSO32)
	/* Align 64bit argument to 64bit boundary.  */
	if (llarg % 2) llarg++;
# endif
# if defined LINUX && (defined X86_64 || defined POWERPC64)
	if (current_personality == 0) {
		tprintf(format, tcp->u_arg[llarg]);
		llarg++;
	} else {
#  ifdef POWERPC64
		/* Align 64bit argument to 64bit boundary.  */
		if (llarg % 2) llarg++;
#  endif
		tprintf(format, LONG_LONG(tcp->u_arg[llarg], tcp->u_arg[llarg + 1]));
		llarg += 2;
	}
# elif defined IA64 || defined ALPHA
	tprintf(format, tcp->u_arg[llarg]);
	llarg++;
# elif defined LINUX_MIPSN32
	tprintf(format, tcp->ext_arg[llarg]);
	llarg++;
# else
	tprintf(format, LONG_LONG(tcp->u_arg[llarg], tcp->u_arg[llarg + 1]));
	llarg += 2;
# endif
	return llarg;
}
#endif

/*
 * Interpret `xlat' as an array of flags
 * print the entries whose bits are on in `flags'
 * return # of flags printed.
 */
int
addflags(xlat, flags)
const struct xlat *xlat;
int flags;
{
	int n;

	for (n = 0; xlat->str; xlat++) {
		if (xlat->val && (flags & xlat->val) == xlat->val) {
			tprintf("|%s", xlat->str);
			flags &= ~xlat->val;
			n++;
		}
	}
	if (flags) {
		tprintf("|%#x", flags);
		n++;
	}
	return n;
}

/*
 * Interpret `xlat' as an array of flags/
 * Print to static string the entries whose bits are on in `flags'
 * Return static string.
 */
const char *
sprintflags(const char *prefix, const struct xlat *xlat, int flags)
{
	static char outstr[1024];
	int found = 0;

	strcpy(outstr, prefix);

	for (; xlat->str; xlat++) {
		if ((flags & xlat->val) == xlat->val) {
			if (found)
				strcat(outstr, "|");
			strcat(outstr, xlat->str);
			flags &= ~xlat->val;
			found = 1;
		}
	}
	if (flags) {
		if (found)
			strcat(outstr, "|");
		sprintf(outstr + strlen(outstr), "%#x", flags);
	}

	return outstr;
}

int
printflags(const struct xlat *xlat, int flags, const char *dflt)
{
	int n;
	const char *sep;

	if (flags == 0 && xlat->val == 0) {
		tprintf("%s", xlat->str);
		return 1;
	}

	sep = "";
	for (n = 0; xlat->str; xlat++) {
		if (xlat->val && (flags & xlat->val) == xlat->val) {
			tprintf("%s%s", sep, xlat->str);
			flags &= ~xlat->val;
			sep = "|";
			n++;
		}
	}

	if (n) {
		if (flags) {
			tprintf("%s%#x", sep, flags);
			n++;
		}
	} else {
		if (flags) {
			tprintf("%#x", flags);
			if (dflt)
				tprintf(" /* %s */", dflt);
		} else {
			if (dflt)
				tprintf("0");
		}
	}

	return n;
}

void
printnum(struct tcb *tcp, long addr, const char *fmt)
{
	long num;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (umove(tcp, addr, &num) < 0) {
		tprintf("%#lx", addr);
		return;
	}
	tprintf("[");
	tprintf(fmt, num);
	tprintf("]");
}

void
printnum_int(struct tcb *tcp, long addr, const char *fmt)
{
	int num;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (umove(tcp, addr, &num) < 0) {
		tprintf("%#lx", addr);
		return;
	}
	tprintf("[");
	tprintf(fmt, num);
	tprintf("]");
}

void
printfd(struct tcb *tcp, int fd)
{
	tprintf("%d", fd);
}

void
printuid(text, uid)
const char *text;
unsigned long uid;
{
	tprintf("%s", text);
	tprintf((uid == -1) ? "%ld" : "%lu", uid);
}

static char path[MAXPATHLEN + 1];

/*
 * Quote string `instr' of length `size'
 * Write up to (3 + `size' * 4) bytes to `outstr' buffer.
 * If `len' < 0, treat `instr' as a NUL-terminated string
 * and quote at most (`size' - 1) bytes.
 */
static int
string_quote(const char *instr, char *outstr, int len, int size)
{
	const unsigned char *ustr = (const unsigned char *) instr;
	char *s = outstr;
	int usehex = 0, c, i;

	if (xflag > 1)
		usehex = 1;
	else if (xflag) {
		/* Check for presence of symbol which require
		   to hex-quote the whole string. */
		for (i = 0; i < size; ++i) {
			c = ustr[i];
			/* Check for NUL-terminated string. */
			if (len < 0) {
				if (c == '\0')
					break;
				/* Quote at most size - 1 bytes. */
				if (i == size - 1)
					continue;
			}
			if (!isprint(c) && !isspace(c)) {
				usehex = 1;
				break;
			}
		}
	}

	*s++ = '\"';

	if (usehex) {
		/* Hex-quote the whole string. */
		for (i = 0; i < size; ++i) {
			c = ustr[i];
			/* Check for NUL-terminated string. */
			if (len < 0) {
				if (c == '\0')
					break;
				/* Quote at most size - 1 bytes. */
				if (i == size - 1)
					continue;
			}
			sprintf(s, "\\x%02x", c);
			s += 4;
		}
	} else {
		for (i = 0; i < size; ++i) {
			c = ustr[i];
			/* Check for NUL-terminated string. */
			if (len < 0) {
				if (c == '\0')
					break;
				/* Quote at most size - 1 bytes. */
				if (i == size - 1)
					continue;
			}
			switch (c) {
				case '\"': case '\\':
					*s++ = '\\';
					*s++ = c;
					break;
				case '\f':
					*s++ = '\\';
					*s++ = 'f';
					break;
				case '\n':
					*s++ = '\\';
					*s++ = 'n';
					break;
				case '\r':
					*s++ = '\\';
					*s++ = 'r';
					break;
				case '\t':
					*s++ = '\\';
					*s++ = 't';
					break;
				case '\v':
					*s++ = '\\';
					*s++ = 'v';
					break;
				default:
					if (isprint(c))
						*s++ = c;
					else if (i + 1 < size
						 && isdigit(ustr[i + 1])) {
						sprintf(s, "\\%03o", c);
						s += 4;
					} else {
						sprintf(s, "\\%o", c);
						s += strlen(s);
					}
					break;
			}
		}
	}

	*s++ = '\"';
	*s = '\0';

	/* Return nonzero if the string was unterminated.  */
	return i == size;
}

/*
 * Print path string specified by address `addr' and length `n'.
 * If path length exceeds `n', append `...' to the output.
 */
void
printpathn(struct tcb *tcp, long addr, int n)
{
	if (!addr) {
		tprintf("NULL");
		return;
	}

	/* Cap path length to the path buffer size,
	   and NUL-terminate the buffer. */
	if (n > sizeof path - 1)
		n = sizeof path - 1;
	path[n] = '\0';

	/* Fetch one byte more to find out whether path length > n. */
	if (umovestr(tcp, addr, n + 1, path) < 0)
		tprintf("%#lx", addr);
	else {
		static char outstr[4*(sizeof path - 1) + sizeof "\"...\""];
		int trunc = (path[n] != '\0');

		if (trunc)
			path[n] = '\0';
		(void) string_quote(path, outstr, -1, n + 1);
		if (trunc)
			strcat(outstr, "...");
		tprintf("%s", outstr);
	}
}

void
printpath(struct tcb *tcp, long addr)
{
	printpathn(tcp, addr, sizeof path - 1);
}

/*
 * Print string specified by address `addr' and length `len'.
 * If `len' < 0, treat the string as a NUL-terminated string.
 * If string length exceeds `max_strlen', append `...' to the output.
 */
void
printstr(struct tcb *tcp, long addr, int len)
{
	static char *str = NULL;
	static char *outstr;
	int size;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	/* Allocate static buffers if they are not allocated yet. */
	if (!str)
		str = malloc(max_strlen + 1);
	if (!outstr)
		outstr = malloc(4 * max_strlen + sizeof "\"...\"");
	if (!str || !outstr) {
		fprintf(stderr, "out of memory\n");
		tprintf("%#lx", addr);
		return;
	}

	if (len < 0) {
		/*
		 * Treat as a NUL-terminated string: fetch one byte more
		 * because string_quote() quotes one byte less.
		 */
		size = max_strlen + 1;
		str[max_strlen] = '\0';
		if (umovestr(tcp, addr, size, str) < 0) {
			tprintf("%#lx", addr);
			return;
		}
	}
	else {
		size = MIN(len, max_strlen);
		if (umoven(tcp, addr, size, str) < 0) {
			tprintf("%#lx", addr);
			return;
		}
	}

	if (string_quote(str, outstr, len, size) &&
	    (len < 0 || len > max_strlen))
		strcat(outstr, "...");

	tprintf("%s", outstr);
}

#if HAVE_SYS_UIO_H
void
dumpiov(tcp, len, addr)
struct tcb * tcp;
int len;
long addr;
{
#if defined(LINUX) && SUPPORTED_PERSONALITIES > 1
	union {
		struct { u_int32_t base; u_int32_t len; } *iov32;
		struct { u_int64_t base; u_int64_t len; } *iov64;
	} iovu;
#define iov iovu.iov64
#define sizeof_iov \
  (personality_wordsize[current_personality] == 4 \
   ? sizeof(*iovu.iov32) : sizeof(*iovu.iov64))
#define iov_iov_base(i) \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iovu.iov32[i].base : iovu.iov64[i].base)
#define iov_iov_len(i) \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iovu.iov32[i].len : iovu.iov64[i].len)
#else
	struct iovec *iov;
#define sizeof_iov sizeof(*iov)
#define iov_iov_base(i) iov[i].iov_base
#define iov_iov_len(i) iov[i].iov_len
#endif
	int i;
	unsigned long size;

	size = sizeof_iov * (unsigned long) len;
	if (size / sizeof_iov != len
	    || (iov = malloc(size)) == NULL) {
		fprintf(stderr, "out of memory\n");
		return;
	}
	if (umoven(tcp, addr, size, (char *) iov) >= 0) {
		for (i = 0; i < len; i++) {
			/* include the buffer number to make it easy to
			 * match up the trace with the source */
			tprintf(" * %lu bytes in buffer %d\n",
				(unsigned long)iov_iov_len(i), i);
			dumpstr(tcp, (long) iov_iov_base(i),
				iov_iov_len(i));
		}
	}
	free((char *) iov);
#undef sizeof_iov
#undef iov_iov_base
#undef iov_iov_len
#undef iov
}
#endif

void
dumpstr(tcp, addr, len)
struct tcb *tcp;
long addr;
int len;
{
	static int strsize = -1;
	static unsigned char *str;
	static char outstr[80];
	char *s;
	int i, j;

	if (strsize < len) {
		if (str)
			free(str);
		if ((str = malloc(len)) == NULL) {
			fprintf(stderr, "out of memory\n");
			return;
		}
		strsize = len;
	}

	if (umoven(tcp, addr, len, (char *) str) < 0)
		return;

	for (i = 0; i < len; i += 16) {
		s = outstr;
		sprintf(s, " | %05x ", i);
		s += 9;
		for (j = 0; j < 16; j++) {
			if (j == 8)
				*s++ = ' ';
			if (i + j < len) {
				sprintf(s, " %02x", str[i + j]);
				s += 3;
			}
			else {
				*s++ = ' '; *s++ = ' '; *s++ = ' ';
			}
		}
		*s++ = ' '; *s++ = ' ';
		for (j = 0; j < 16; j++) {
			if (j == 8)
				*s++ = ' ';
			if (i + j < len) {
				if (isprint(str[i + j]))
					*s++ = str[i + j];
				else
					*s++ = '.';
			}
			else
				*s++ = ' ';
		}
		tprintf("%s |\n", outstr);
	}
}

#define PAGMASK	(~(PAGSIZ - 1))
/*
 * move `len' bytes of data from process `pid'
 * at address `addr' to our space at `laddr'
 */
int
umoven(struct tcb *tcp, long addr, int len, char *laddr)
{
#ifdef LINUX
	int pid = tcp->pid;
	int n, m;
	int started = 0;
	union {
		long val;
		char x[sizeof(long)];
	} u;

	if (addr & (sizeof(long) - 1)) {
		/* addr not a multiple of sizeof(long) */
		n = addr - (addr & -sizeof(long)); /* residue */
		addr &= -sizeof(long); /* residue */
		errno = 0;
		u.val = ptrace(PTRACE_PEEKDATA, pid, (char *) addr, 0);
		if (errno) {
			if (started && (errno==EPERM || errno==EIO)) {
				/* Ran into 'end of memory' - stupid "printpath" */
				return 0;
			}
			/* But if not started, we had a bogus address. */
			if (addr != 0 && errno != EIO && errno != ESRCH)
				perror("ptrace: umoven");
			return -1;
		}
		started = 1;
		memcpy(laddr, &u.x[n], m = MIN(sizeof(long) - n, len));
		addr += sizeof(long), laddr += m, len -= m;
	}
	while (len) {
		errno = 0;
		u.val = ptrace(PTRACE_PEEKDATA, pid, (char *) addr, 0);
		if (errno) {
			if (started && (errno==EPERM || errno==EIO)) {
				/* Ran into 'end of memory' - stupid "printpath" */
				return 0;
			}
			if (addr != 0 && errno != EIO && errno != ESRCH)
				perror("ptrace: umoven");
			return -1;
		}
		started = 1;
		memcpy(laddr, u.x, m = MIN(sizeof(long), len));
		addr += sizeof(long), laddr += m, len -= m;
	}
#endif /* LINUX */

#ifdef SUNOS4
	int pid = tcp->pid;
	int n;

	while (len) {
		n = MIN(len, PAGSIZ);
		n = MIN(n, ((addr + PAGSIZ) & PAGMASK) - addr);
		if (ptrace(PTRACE_READDATA, pid,
			   (char *) addr, len, laddr) < 0) {
			if (errno != ESRCH) {
				perror("umoven: ptrace(PTRACE_READDATA, ...)");
				abort();
			}
			return -1;
		}
		len -= n;
		addr += n;
		laddr += n;
	}
#endif /* SUNOS4 */

#ifdef USE_PROCFS
#ifdef HAVE_MP_PROCFS
	int fd = tcp->pfd_as;
#else
	int fd = tcp->pfd;
#endif
	lseek(fd, addr, SEEK_SET);
	if (read(fd, laddr, len) == -1)
		return -1;
#endif /* USE_PROCFS */

	return 0;
}

/*
 * like `umove' but make the additional effort of looking
 * for a terminating zero byte.
 */
int
umovestr(struct tcb *tcp, long addr, int len, char *laddr)
{
#ifdef USE_PROCFS
#ifdef HAVE_MP_PROCFS
	int fd = tcp->pfd_as;
#else
	int fd = tcp->pfd;
#endif
	/* Some systems (e.g. FreeBSD) can be upset if we read off the
	   end of valid memory,  avoid this by trying to read up
	   to page boundaries.  But we don't know what a page is (and
	   getpagesize(2) (if it exists) doesn't necessarily return
	   hardware page size).  Assume all pages >= 1024 (a-historical
	   I know) */

	int page = 1024; 	/* How to find this? */
	int move = page - (addr & (page - 1));
	int left = len;

	lseek(fd, addr, SEEK_SET);

	while (left) {
		if (move > left) move = left;
		if ((move = read(fd, laddr, move)) <= 0)
			return left != len ? 0 : -1;
		if (memchr (laddr, 0, move)) break;
		left -= move;
		laddr += move;
		addr += move;
		move = page;
	}
#else /* !USE_PROCFS */
	int started = 0;
	int pid = tcp->pid;
	int i, n, m;
	union {
		long val;
		char x[sizeof(long)];
	} u;

	if (addr & (sizeof(long) - 1)) {
		/* addr not a multiple of sizeof(long) */
		n = addr - (addr & -sizeof(long)); /* residue */
		addr &= -sizeof(long); /* residue */
		errno = 0;
		u.val = ptrace(PTRACE_PEEKDATA, pid, (char *)addr, 0);
		if (errno) {
			if (started && (errno==EPERM || errno==EIO)) {
				/* Ran into 'end of memory' - stupid "printpath" */
				return 0;
			}
			if (addr != 0 && errno != EIO && errno != ESRCH)
				perror("umovestr");
			return -1;
		}
		started = 1;
		memcpy(laddr, &u.x[n], m = MIN(sizeof(long)-n,len));
		while (n & (sizeof(long) - 1))
			if (u.x[n++] == '\0')
				return 0;
		addr += sizeof(long), laddr += m, len -= m;
	}
	while (len) {
		errno = 0;
		u.val = ptrace(PTRACE_PEEKDATA, pid, (char *)addr, 0);
		if (errno) {
			if (started && (errno==EPERM || errno==EIO)) {
				/* Ran into 'end of memory' - stupid "printpath" */
				return 0;
			}
			if (addr != 0 && errno != EIO && errno != ESRCH)
				perror("umovestr");
			return -1;
		}
		started = 1;
		memcpy(laddr, u.x, m = MIN(sizeof(long), len));
		for (i = 0; i < sizeof(long); i++)
			if (u.x[i] == '\0')
				return 0;

		addr += sizeof(long), laddr += m, len -= m;
	}
#endif /* !USE_PROCFS */
	return 0;
}

#ifdef LINUX
# if !defined (SPARC) && !defined(SPARC64)
#  define PTRACE_WRITETEXT	101
#  define PTRACE_WRITEDATA	102
# endif /* !SPARC && !SPARC64 */
#endif /* LINUX */

#ifdef SUNOS4

static int
uload(cmd, pid, addr, len, laddr)
int cmd;
int pid;
long addr;
int len;
char *laddr;
{
	int peek, poke;
	int n, m;
	union {
		long val;
		char x[sizeof(long)];
	} u;

	if (cmd == PTRACE_WRITETEXT) {
		peek = PTRACE_PEEKTEXT;
		poke = PTRACE_POKETEXT;
	}
	else {
		peek = PTRACE_PEEKDATA;
		poke = PTRACE_POKEDATA;
	}
	if (addr & (sizeof(long) - 1)) {
		/* addr not a multiple of sizeof(long) */
		n = addr - (addr & -sizeof(long)); /* residue */
		addr &= -sizeof(long);
		errno = 0;
		u.val = ptrace(peek, pid, (char *) addr, 0);
		if (errno) {
			perror("uload: POKE");
			return -1;
		}
		memcpy(&u.x[n], laddr, m = MIN(sizeof(long) - n, len));
		if (ptrace(poke, pid, (char *)addr, u.val) < 0) {
			perror("uload: POKE");
			return -1;
		}
		addr += sizeof(long), laddr += m, len -= m;
	}
	while (len) {
		if (len < sizeof(long))
			u.val = ptrace(peek, pid, (char *) addr, 0);
		memcpy(u.x, laddr, m = MIN(sizeof(long), len));
		if (ptrace(poke, pid, (char *) addr, u.val) < 0) {
			perror("uload: POKE");
			return -1;
		}
		addr += sizeof(long), laddr += m, len -= m;
	}
	return 0;
}

int
tload(pid, addr, len, laddr)
int pid;
int addr, len;
char *laddr;
{
	return uload(PTRACE_WRITETEXT, pid, addr, len, laddr);
}

int
dload(pid, addr, len, laddr)
int pid;
int addr;
int len;
char *laddr;
{
	return uload(PTRACE_WRITEDATA, pid, addr, len, laddr);
}

#endif /* SUNOS4 */

#ifndef USE_PROCFS

int
upeek(tcp, off, res)
struct tcb *tcp;
long off;
long *res;
{
	long val;

# ifdef SUNOS4_KERNEL_ARCH_KLUDGE
	{
		static int is_sun4m = -1;
		struct utsname name;

		/* Round up the usual suspects. */
		if (is_sun4m == -1) {
			if (uname(&name) < 0) {
				perror("upeek: uname?");
				exit(1);
			}
			is_sun4m = strcmp(name.machine, "sun4m") == 0;
			if (is_sun4m) {
				const struct xlat *x;

				for (x = struct_user_offsets; x->str; x++)
					x->val += 1024;
			}
		}
		if (is_sun4m)
			off += 1024;
	}
# endif /* SUNOS4_KERNEL_ARCH_KLUDGE */
	errno = 0;
	val = do_ptrace(PTRACE_PEEKUSER, tcp, (char *) off, 0);
	if (val == -1 && errno) {
		if (errno != ESRCH) {
			char buf[60];
			sprintf(buf,"upeek: ptrace(PTRACE_PEEKUSER,%d,%lu,0)", tcp->pid, off);
			perror(buf);
		}
		return -1;
	}
	*res = val;
	return 0;
}

#endif /* !USE_PROCFS */

void
printcall(struct tcb *tcp)
{
#define PRINTBADPC tprintf(sizeof(long) == 4 ? "[????????] " : \
			   sizeof(long) == 8 ? "[????????????????] " : \
			   NULL /* crash */)

#ifdef LINUX
# ifdef I386
	long eip;

	if (upeek(tcp, 4*EIP, &eip) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", eip);

# elif defined(S390) || defined(S390X)
	long psw;
	if(upeek(tcp,PT_PSWADDR,&psw) < 0) {
		PRINTBADPC;
		return;
	}
#  ifdef S390
	tprintf("[%08lx] ", psw);
#  elif S390X
	tprintf("[%16lx] ", psw);
#  endif

# elif defined(X86_64)
	long rip;

	if (upeek(tcp, 8*RIP, &rip) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%16lx] ", rip);
# elif defined(IA64)
	long ip;

	if (upeek(tcp, PT_B0, &ip) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", ip);
# elif defined(POWERPC)
	long pc;

	if (upeek(tcp, sizeof(unsigned long)*PT_NIP, &pc) < 0) {
		PRINTBADPC;
		return;
	}
#  ifdef POWERPC64
	tprintf("[%016lx] ", pc);
#  else
	tprintf("[%08lx] ", pc);
#  endif
# elif defined(M68K)
	long pc;

	if (upeek(tcp, 4*PT_PC, &pc) < 0) {
		tprintf ("[????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(ALPHA)
	long pc;

	if (upeek(tcp, REG_PC, &pc) < 0) {
		tprintf ("[????????????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(SPARC) || defined(SPARC64)
	struct pt_regs regs;
	if (ptrace(PTRACE_GETREGS,tcp->pid,(char *)&regs,0) < 0) {
		PRINTBADPC;
		return;
	}
#  if defined(SPARC64)
	tprintf("[%08lx] ", regs.tpc);
#  else
	tprintf("[%08lx] ", regs.pc);
#  endif
# elif defined(HPPA)
	long pc;

	if(upeek(tcp,PT_IAOQ0,&pc) < 0) {
		tprintf ("[????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(MIPS)
	long pc;

	if (upeek(tcp, REG_EPC, &pc) < 0) {
		tprintf ("[????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(SH)
	long pc;

	if (upeek(tcp, 4*REG_PC, &pc) < 0) {
		tprintf ("[????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(SH64)
	long pc;

	if (upeek(tcp, REG_PC, &pc) < 0) {
		tprintf ("[????????????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(ARM)
	long pc;

	if (upeek(tcp, 4*15, &pc) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(AVR32)
	long pc;

	if (upeek(tcp, REG_PC, &pc) < 0) {
		tprintf("[????????] ");
		return;
	}
	tprintf("[%08lx] ", pc);
# elif defined(BFIN)
	long pc;

	if (upeek(tcp, PT_PC, &pc) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", pc);
#elif defined(CRISV10)
	long pc;

	if (upeek(tcp, 4*PT_IRP, &pc) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", pc);
#elif defined(CRISV32)
	long pc;

	if (upeek(tcp, 4*PT_ERP, &pc) < 0) {
		PRINTBADPC;
		return;
	}
	tprintf("[%08lx] ", pc);
# endif /* architecture */
#endif /* LINUX */

#ifdef SUNOS4
	struct regs regs;

	if (ptrace(PTRACE_GETREGS, tcp->pid, (char *) &regs, 0) < 0) {
		perror("printcall: ptrace(PTRACE_GETREGS, ...)");
		PRINTBADPC;
		return;
	}
	tprintf("[%08x] ", regs.r_o7);
#endif /* SUNOS4 */

#ifdef SVR4
	/* XXX */
	PRINTBADPC;
#endif

#ifdef FREEBSD
	struct reg regs;
	pread(tcp->pfd_reg, &regs, sizeof(regs), 0);
	tprintf("[%08x] ", regs.r_eip);
#endif /* FREEBSD */
}


/*
 * These #if's are huge, please indent them correctly.
 * It's easy to get confused otherwise.
 */
#ifndef USE_PROCFS

#ifdef LINUX

#  include "syscall.h"

#  include <sys/syscall.h>
#  ifndef CLONE_PTRACE
#   define CLONE_PTRACE    0x00002000
#  endif
#  ifndef CLONE_VFORK
#   define CLONE_VFORK     0x00004000
#  endif
#  ifndef CLONE_VM
#   define CLONE_VM        0x00000100
#  endif
#  ifndef CLONE_STOPPED
#   define CLONE_STOPPED   0x02000000
#  endif

#  ifdef IA64

/* We don't have fork()/vfork() syscalls on ia64 itself, but the ia32
   subsystem has them for x86... */
#   define SYS_fork	2
#   define SYS_vfork	190

typedef unsigned long *arg_setup_state;

static int
arg_setup(struct tcb *tcp, arg_setup_state *state)
{
	unsigned long cfm, sof, sol;
	long bsp;

	if (ia32) {
		/* Satisfy a false GCC warning.  */
		*state = NULL;
		return 0;
	}

	if (upeek(tcp, PT_AR_BSP, &bsp) < 0)
		return -1;
	if (upeek(tcp, PT_CFM, (long *) &cfm) < 0)
		return -1;

	sof = (cfm >> 0) & 0x7f;
	sol = (cfm >> 7) & 0x7f;
	bsp = (long) ia64_rse_skip_regs((unsigned long *) bsp, -sof + sol);

	*state = (unsigned long *) bsp;
	return 0;
}

#   define arg_finish_change(tcp, state)	0

#   ifdef SYS_fork
static int
get_arg0 (struct tcb *tcp, arg_setup_state *state, long *valp)
{
	int ret;

	if (ia32)
		ret = upeek (tcp, PT_R11, valp);
	else
		ret = umoven (tcp,
			      (unsigned long) ia64_rse_skip_regs(*state, 0),
			      sizeof(long), (void *) valp);
	return ret;
}

static int
get_arg1 (struct tcb *tcp, arg_setup_state *state, long *valp)
{
	int ret;

	if (ia32)
		ret = upeek (tcp, PT_R9, valp);
	else
		ret = umoven (tcp,
			      (unsigned long) ia64_rse_skip_regs(*state, 1),
			      sizeof(long), (void *) valp);
	return ret;
}
#   endif

static int
set_arg0 (struct tcb *tcp, arg_setup_state *state, long val)
{
	int req = PTRACE_POKEDATA;
	void *ap;

	if (ia32) {
		ap = (void *) (intptr_t) PT_R11;	 /* r11 == EBX */
		req = PTRACE_POKEUSER;
	} else
		ap = ia64_rse_skip_regs(*state, 0);
	errno = 0;
	ptrace(req, tcp->pid, ap, val);
	return errno ? -1 : 0;
}

static int
set_arg1 (struct tcb *tcp, arg_setup_state *state, long val)
{
	int req = PTRACE_POKEDATA;
	void *ap;

	if (ia32) {
		ap = (void *) (intptr_t) PT_R9;		/* r9 == ECX */
		req = PTRACE_POKEUSER;
	} else
		ap = ia64_rse_skip_regs(*state, 1);
	errno = 0;
	ptrace(req, tcp->pid, ap, val);
	return errno ? -1 : 0;
}

/* ia64 does not return the input arguments from functions (and syscalls)
   according to ia64 RSE (Register Stack Engine) behavior.  */

#   define restore_arg0(tcp, state, val) ((void) (state), 0)
#   define restore_arg1(tcp, state, val) ((void) (state), 0)

#  elif defined (SPARC) || defined (SPARC64)

typedef struct pt_regs arg_setup_state;

#   define arg_setup(tcp, state) \
    (ptrace (PTRACE_GETREGS, tcp->pid, (char *) (state), 0))
#   define arg_finish_change(tcp, state) \
    (ptrace (PTRACE_SETREGS, tcp->pid, (char *) (state), 0))

#   define get_arg0(tcp, state, valp) (*(valp) = (state)->u_regs[U_REG_O0], 0)
#   define get_arg1(tcp, state, valp) (*(valp) = (state)->u_regs[U_REG_O1], 0)
#   define set_arg0(tcp, state, val) ((state)->u_regs[U_REG_O0] = (val), 0)
#   define set_arg1(tcp, state, val) ((state)->u_regs[U_REG_O1] = (val), 0)
#   define restore_arg0(tcp, state, val) 0

#  else /* other architectures */

#   if defined S390 || defined S390X
/* Note: this is only true for the `clone' system call, which handles
   arguments specially.  We could as well say that its first two arguments
   are swapped relative to other architectures, but that would just be
   another #ifdef in the calls.  */
#    define arg0_offset	PT_GPR3
#    define arg1_offset	PT_ORIGGPR2
#    define restore_arg0(tcp, state, val) ((void) (state), 0)
#    define restore_arg1(tcp, state, val) ((void) (state), 0)
#    define arg0_index	1
#    define arg1_index	0
#   elif defined (ALPHA) || defined (MIPS)
#    define arg0_offset	REG_A0
#    define arg1_offset	(REG_A0+1)
#   elif defined (AVR32)
#    define arg0_offset	(REG_R12)
#    define arg1_offset	(REG_R11)
#   elif defined (POWERPC)
#    define arg0_offset	(sizeof(unsigned long)*PT_R3)
#    define arg1_offset	(sizeof(unsigned long)*PT_R4)
#    define restore_arg0(tcp, state, val) ((void) (state), 0)
#   elif defined (HPPA)
#    define arg0_offset	 PT_GR26
#    define arg1_offset	 (PT_GR26-4)
#   elif defined (X86_64)
#    define arg0_offset	((long)(8*(current_personality ? RBX : RDI)))
#    define arg1_offset	((long)(8*(current_personality ? RCX : RSI)))
#   elif defined (SH)
#    define arg0_offset	(4*(REG_REG0+4))
#    define arg1_offset	(4*(REG_REG0+5))
#   elif defined (SH64)
    /* ABI defines arg0 & 1 in r2 & r3 */
#    define arg0_offset   (REG_OFFSET+16)
#    define arg1_offset   (REG_OFFSET+24)
#    define restore_arg0(tcp, state, val) 0
#   elif defined CRISV10 || defined CRISV32
#    define arg0_offset   (4*PT_R11)
#    define arg1_offset   (4*PT_ORIG_R10)
#    define restore_arg0(tcp, state, val) 0
#    define restore_arg1(tcp, state, val) 0
#    define arg0_index   1
#    define arg1_index   0
#   else
#    define arg0_offset	0
#    define arg1_offset	4
#    if defined ARM
#     define restore_arg0(tcp, state, val) 0
#    endif
#   endif

typedef int arg_setup_state;

#   define arg_setup(tcp, state) (0)
#   define arg_finish_change(tcp, state)	0
#   define get_arg0(tcp, cookie, valp) \
    (upeek ((tcp), arg0_offset, (valp)))
#   define get_arg1(tcp, cookie, valp) \
    (upeek ((tcp), arg1_offset, (valp)))

static int
set_arg0 (struct tcb *tcp, void *cookie, long val)
{
	return ptrace (PTRACE_POKEUSER, tcp->pid, (char*)arg0_offset, val);
}

static int
set_arg1 (struct tcb *tcp, void *cookie, long val)
{
	return ptrace (PTRACE_POKEUSER, tcp->pid, (char*)arg1_offset, val);
}

#  endif /* architectures */

#  ifndef restore_arg0
#   define restore_arg0(tcp, state, val) set_arg0((tcp), (state), (val))
#  endif
#  ifndef restore_arg1
#   define restore_arg1(tcp, state, val) set_arg1((tcp), (state), (val))
#  endif

#  ifndef arg0_index
#   define arg0_index 0
#   define arg1_index 1
#  endif

int
setbpt(struct tcb *tcp)
{
	static int clone_scno[SUPPORTED_PERSONALITIES] = { SYS_clone };
	arg_setup_state state;

	if (tcp->flags & TCB_BPTSET) {
		fprintf(stderr, "PANIC: TCB already set in pid %u\n", tcp->pid);
		return -1;
	}

	/*
	 * It's a silly kludge to initialize this with a search at runtime.
	 * But it's better than maintaining another magic thing in the
	 * godforsaken tables.
	 */
	if (clone_scno[current_personality] == 0) {
		int i;
		for (i = 0; i < nsyscalls; ++i)
			if (sysent[i].sys_func == sys_clone) {
				clone_scno[current_personality] = i;
				break;
			}
	}

	switch (known_scno(tcp)) {
#  ifdef SYS_vfork
	case SYS_vfork:
#  endif
#  ifdef SYS_fork
	case SYS_fork:
#  endif
#  if defined SYS_fork || defined SYS_vfork
		if (arg_setup (tcp, &state) < 0
		    || get_arg0 (tcp, &state, &tcp->inst[0]) < 0
		    || get_arg1 (tcp, &state, &tcp->inst[1]) < 0
		    || change_syscall(tcp, clone_scno[current_personality]) < 0
		    || set_arg0 (tcp, &state, CLONE_PTRACE|SIGCHLD) < 0
		    || set_arg1 (tcp, &state, 0) < 0
		    || arg_finish_change (tcp, &state) < 0)
			return -1;
		tcp->u_arg[arg0_index] = CLONE_PTRACE|SIGCHLD;
		tcp->u_arg[arg1_index] = 0;
		tcp->flags |= TCB_BPTSET;
		return 0;
#  endif

	case SYS_clone:
#  ifdef SYS_clone2
	case SYS_clone2:
#  endif
		/* ia64 calls directly `clone (CLONE_VFORK | CLONE_VM)'
		   contrary to x86 SYS_vfork above.  Even on x86 we turn the
		   vfork semantics into plain fork - each application must not
		   depend on the vfork specifics according to POSIX.  We would
		   hang waiting for the parent resume otherwise.  We need to
		   clear also CLONE_VM but only in the CLONE_VFORK case as
		   otherwise we would break pthread_create.  */

		if ((arg_setup (tcp, &state) < 0
		    || set_arg0 (tcp, &state,
				 (tcp->u_arg[arg0_index] | CLONE_PTRACE)
				 & ~(tcp->u_arg[arg0_index] & CLONE_VFORK
				     ? CLONE_VFORK | CLONE_VM : 0)) < 0
		    || arg_finish_change (tcp, &state) < 0))
			return -1;
		tcp->flags |= TCB_BPTSET;
		tcp->inst[0] = tcp->u_arg[arg0_index];
		tcp->inst[1] = tcp->u_arg[arg1_index];
		return 0;

	default:
		fprintf(stderr, "PANIC: setbpt for syscall %ld on %u???\n",
			tcp->scno, tcp->pid);
		break;
	}

	return -1;
}

int
clearbpt(tcp)
struct tcb *tcp;
{
	arg_setup_state state;
	if (arg_setup (tcp, &state) < 0
	    || restore_arg0 (tcp, &state, tcp->inst[0]) < 0
	    || restore_arg1 (tcp, &state, tcp->inst[1]) < 0
	    || arg_finish_change (tcp, &state))
		if (errno != ESRCH) return -1;
	tcp->flags &= ~TCB_BPTSET;
	return 0;
}

# else /* !defined LINUX */

int
setbpt(tcp)
struct tcb *tcp;
{
#  ifdef SUNOS4
#   ifdef SPARC	/* This code is slightly sparc specific */

	struct regs regs;
#    define BPT	0x91d02001	/* ta	1 */
#    define LOOP	0x10800000	/* ba	0 */
#    define LOOPA	0x30800000	/* ba,a	0 */
#    define NOP	0x01000000
#    if LOOPA
	static int loopdeloop[1] = {LOOPA};
#    else
	static int loopdeloop[2] = {LOOP, NOP};
#    endif

	if (tcp->flags & TCB_BPTSET) {
		fprintf(stderr, "PANIC: TCB already set in pid %u\n", tcp->pid);
		return -1;
	}
	if (ptrace(PTRACE_GETREGS, tcp->pid, (char *)&regs, 0) < 0) {
		perror("setbpt: ptrace(PTRACE_GETREGS, ...)");
		return -1;
	}
	tcp->baddr = regs.r_o7 + 8;
	if (ptrace(PTRACE_READTEXT, tcp->pid, (char *)tcp->baddr,
				sizeof tcp->inst, (char *)tcp->inst) < 0) {
		perror("setbpt: ptrace(PTRACE_READTEXT, ...)");
		return -1;
	}

	/*
	 * XXX - BRUTAL MODE ON
	 * We cannot set a real BPT in the child, since it will not be
	 * traced at the moment it will reach the trap and would probably
	 * die with a core dump.
	 * Thus, we are force our way in by taking out two instructions
	 * and insert an eternal loop in stead, in expectance of the SIGSTOP
	 * generated by out PTRACE_ATTACH.
	 * Of cause, if we evaporate ourselves in the middle of all this...
	 */
	if (ptrace(PTRACE_WRITETEXT, tcp->pid, (char *) tcp->baddr,
			sizeof loopdeloop, (char *) loopdeloop) < 0) {
		perror("setbpt: ptrace(PTRACE_WRITETEXT, ...)");
		return -1;
	}
	tcp->flags |= TCB_BPTSET;

#   endif /* SPARC */
#  endif /* SUNOS4 */

	return 0;
}

int
clearbpt(tcp)
struct tcb *tcp;
{
#  ifdef SUNOS4
#   ifdef SPARC

#    if !LOOPA
	struct regs regs;
#    endif

	if (!(tcp->flags & TCB_BPTSET)) {
		fprintf(stderr, "PANIC: TCB not set in pid %u\n", tcp->pid);
		return -1;
	}
	if (ptrace(PTRACE_WRITETEXT, tcp->pid, (char *) tcp->baddr,
				sizeof tcp->inst, (char *) tcp->inst) < 0) {
		perror("clearbtp: ptrace(PTRACE_WRITETEXT, ...)");
		return -1;
	}
	tcp->flags &= ~TCB_BPTSET;

#    if !LOOPA
	/*
	 * Since we don't have a single instruction breakpoint, we may have
	 * to adjust the program counter after removing our `breakpoint'.
	 */
	if (ptrace(PTRACE_GETREGS, tcp->pid, (char *)&regs, 0) < 0) {
		perror("clearbpt: ptrace(PTRACE_GETREGS, ...)");
		return -1;
	}
	if ((regs.r_pc < tcp->baddr) ||
				(regs.r_pc > tcp->baddr + 4)) {
		/* The breakpoint has not been reached yet */
		if (debug)
			fprintf(stderr,
				"NOTE: PC not at bpt (pc %#x baddr %#x)\n",
					regs.r_pc, tcp->baddr);
		return 0;
	}
	if (regs.r_pc != tcp->baddr)
		if (debug)
			fprintf(stderr, "NOTE: PC adjusted (%#x -> %#x\n",
				regs.r_pc, tcp->baddr);

	regs.r_pc = tcp->baddr;
	if (ptrace(PTRACE_SETREGS, tcp->pid, (char *)&regs, 0) < 0) {
		perror("clearbpt: ptrace(PTRACE_SETREGS, ...)");
		return -1;
	}
#    endif /* LOOPA */
#   endif /* SPARC */
#  endif /* SUNOS4 */

	return 0;
}

# endif /* !defined LINUX */

#endif /* !USE_PROCFS */


#ifdef SUNOS4

static int
getex(tcp, hdr)
struct tcb *tcp;
struct exec *hdr;
{
	int n;

	for (n = 0; n < sizeof *hdr; n += 4) {
		long res;
		if (upeek(tcp, uoff(u_exdata) + n, &res) < 0)
			return -1;
		memcpy(((char *) hdr) + n, &res, 4);
	}
	if (debug) {
		fprintf(stderr, "[struct exec: magic: %o version %u Mach %o\n",
			hdr->a_magic, hdr->a_toolversion, hdr->a_machtype);
		fprintf(stderr, "Text %lu Data %lu Bss %lu Syms %lu Entry %#lx]\n",
			hdr->a_text, hdr->a_data, hdr->a_bss, hdr->a_syms, hdr->a_entry);
	}
	return 0;
}

int
fixvfork(tcp)
struct tcb *tcp;
{
	int pid = tcp->pid;
	/*
	 * Change `vfork' in a freshly exec'ed dynamically linked
	 * executable's (internal) symbol table to plain old `fork'
	 */

	struct exec hdr;
	struct link_dynamic dyn;
	struct link_dynamic_2 ld;
	char *strtab, *cp;

	if (getex(tcp, &hdr) < 0)
		return -1;
	if (!hdr.a_dynamic)
		return -1;

	if (umove(tcp, (int) N_DATADDR(hdr), &dyn) < 0) {
		fprintf(stderr, "Cannot read DYNAMIC\n");
		return -1;
	}
	if (umove(tcp, (int) dyn.ld_un.ld_2, &ld) < 0) {
		fprintf(stderr, "Cannot read link_dynamic_2\n");
		return -1;
	}
	if ((strtab = malloc((unsigned)ld.ld_symb_size)) == NULL) {
		fprintf(stderr, "out of memory\n");
		return -1;
	}
	if (umoven(tcp, (int)ld.ld_symbols+(int)N_TXTADDR(hdr),
					(int)ld.ld_symb_size, strtab) < 0)
		goto err;

	for (cp = strtab; cp < strtab + ld.ld_symb_size; ) {
		if (strcmp(cp, "_vfork") == 0) {
			if (debug)
				fprintf(stderr, "fixvfork: FOUND _vfork\n");
			strcpy(cp, "_fork");
			break;
		}
		cp += strlen(cp)+1;
	}
	if (cp < strtab + ld.ld_symb_size)
		/*
		 * Write entire symbol table back to avoid
		 * memory alignment bugs in ptrace
		 */
		if (tload(pid, (int)ld.ld_symbols+(int)N_TXTADDR(hdr),
					(int)ld.ld_symb_size, strtab) < 0)
			goto err;

	free(strtab);
	return 0;

err:
	free(strtab);
	return -1;
}

#endif /* SUNOS4 */
