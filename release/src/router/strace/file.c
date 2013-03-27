/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
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

#include <dirent.h>

#ifdef LINUX
struct kernel_dirent {
	unsigned long   d_ino;
	unsigned long   d_off;
	unsigned short  d_reclen;
	char            d_name[1];
};
#else
# define kernel_dirent dirent
#endif

#ifdef LINUX
#  ifdef LINUXSPARC
struct stat {
	unsigned short	st_dev;
	unsigned int	st_ino;
	unsigned short	st_mode;
	short		st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned short	st_rdev;
	unsigned int	st_size;
	int		st_atime;
	unsigned int	__unused1;
	int		st_mtime;
	unsigned int	__unused2;
	int		st_ctime;
	unsigned int	__unused3;
	int		st_blksize;
	int		st_blocks;
	unsigned int	__unused4[2];
};
#if defined(SPARC64)
struct stat_sparc64 {
	unsigned int	st_dev;
	unsigned long	st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	st_rdev;
	long		st_size;
	long		st_atime;
	long		st_mtime;
	long		st_ctime;
	long		st_blksize;
	long		st_blocks;
	unsigned long	__unused4[2];
};
#endif /* SPARC64 */
#    define stat kernel_stat
#    include <asm/stat.h>
#    undef stat
#  else
#    undef dev_t
#    undef ino_t
#    undef mode_t
#    undef nlink_t
#    undef uid_t
#    undef gid_t
#    undef off_t
#    undef loff_t

#    define dev_t __kernel_dev_t
#    define ino_t __kernel_ino_t
#    define mode_t __kernel_mode_t
#    define nlink_t __kernel_nlink_t
#    define uid_t __kernel_uid_t
#    define gid_t __kernel_gid_t
#    define off_t __kernel_off_t
#    define loff_t __kernel_loff_t

#    include <asm/stat.h>

#    undef dev_t
#    undef ino_t
#    undef mode_t
#    undef nlink_t
#    undef uid_t
#    undef gid_t
#    undef off_t
#    undef loff_t

#    define dev_t dev_t
#    define ino_t ino_t
#    define mode_t mode_t
#    define nlink_t nlink_t
#    define uid_t uid_t
#    define gid_t gid_t
#    define off_t off_t
#    define loff_t loff_t
#  endif
#  ifdef HPPA	/* asm-parisc/stat.h defines stat64 */
#    undef stat64
#  endif
#  define stat libc_stat
#  define stat64 libc_stat64
#  include <sys/stat.h>
#  undef stat
#  undef stat64
   /* These might be macros. */
#  undef st_atime
#  undef st_mtime
#  undef st_ctime
#  ifdef HPPA
#    define stat64 hpux_stat64
#  endif
#else
#  include <sys/stat.h>
#endif

#include <fcntl.h>

#ifdef SVR4
#  include <sys/cred.h>
#endif /* SVR4 */

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_LINUX_XATTR_H
#include <linux/xattr.h>
#elif defined linux
#define XATTR_CREATE 1
#define XATTR_REPLACE 2
#endif

#ifdef FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#endif

#if _LFS64_LARGEFILE && (defined(LINUX) || defined(SVR4))
# ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
# else
#  define PRId64 "lld"
#  define PRIu64 "llu"
# endif
#endif

#if HAVE_LONG_LONG_OFF_T
/*
 * Ugly hacks for systems that have typedef long long off_t
 */

#define stat64 stat
#define HAVE_STAT64 1	/* Ugly hack */

#define	sys_stat64	sys_stat
#define sys_fstat64	sys_fstat
#define sys_lstat64	sys_lstat
#define sys_lseek64	sys_lseek
#define sys_truncate64	sys_truncate
#define sys_ftruncate64	sys_ftruncate
#endif

#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif

#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#endif

#ifdef HAVE_SYS_ASYNCH_H
#include <sys/asynch.h>
#endif

#ifdef SUNOS4
#include <ustat.h>
#endif

const struct xlat open_access_modes[] = {
	{ O_RDONLY,	"O_RDONLY"	},
	{ O_WRONLY,	"O_WRONLY"	},
	{ O_RDWR,	"O_RDWR"	},
#ifdef O_ACCMODE
	{ O_ACCMODE,	"O_ACCMODE"	},
#endif
	{ 0,		NULL		},
};

const struct xlat open_mode_flags[] = {
	{ O_CREAT,	"O_CREAT"	},
	{ O_EXCL,	"O_EXCL"	},
	{ O_NOCTTY,	"O_NOCTTY"	},
	{ O_TRUNC,	"O_TRUNC"	},
	{ O_APPEND,	"O_APPEND"	},
	{ O_NONBLOCK,	"O_NONBLOCK"	},
#ifdef O_SYNC
	{ O_SYNC,	"O_SYNC"	},
#endif
#ifdef O_ASYNC
	{ O_ASYNC,	"O_ASYNC"	},
#endif
#ifdef O_DSYNC
	{ O_DSYNC,	"O_DSYNC"	},
#endif
#ifdef O_RSYNC
	{ O_RSYNC,	"O_RSYNC"	},
#endif
#if defined(O_NDELAY) && (O_NDELAY != O_NONBLOCK)
	{ O_NDELAY,	"O_NDELAY"	},
#endif
#ifdef O_PRIV
	{ O_PRIV,	"O_PRIV"	},
#endif
#ifdef O_DIRECT
	{ O_DIRECT,	"O_DIRECT"	},
#endif
#ifdef O_LARGEFILE
# if O_LARGEFILE == 0		/* biarch platforms in 64-bit mode */
#  undef O_LARGEFILE
#  ifdef SPARC64
#   define O_LARGEFILE	0x40000
#  elif defined X86_64 || defined S390X
#   define O_LARGEFILE	0100000
#  endif
# endif
# ifdef O_LARGEFILE
	{ O_LARGEFILE,	"O_LARGEFILE"   },
# endif
#endif
#ifdef O_DIRECTORY
	{ O_DIRECTORY,	"O_DIRECTORY"   },
#endif
#ifdef O_NOFOLLOW
	{ O_NOFOLLOW, 	"O_NOFOLLOW"	},
#endif
#ifdef O_NOATIME
	{ O_NOATIME, 	"O_NOATIME"	},
#endif
#ifdef O_CLOEXEC
	{ O_CLOEXEC,	"O_CLOEXEC"	},
#endif

#ifdef FNDELAY
	{ FNDELAY,	"FNDELAY"	},
#endif
#ifdef FAPPEND
	{ FAPPEND,	"FAPPEND"	},
#endif
#ifdef FMARK
	{ FMARK,	"FMARK"		},
#endif
#ifdef FDEFER
	{ FDEFER,	"FDEFER"	},
#endif
#ifdef FASYNC
	{ FASYNC,	"FASYNC"	},
#endif
#ifdef FSHLOCK
	{ FSHLOCK,	"FSHLOCK"	},
#endif
#ifdef FEXLOCK
	{ FEXLOCK,	"FEXLOCK"	},
#endif
#ifdef FCREAT
	{ FCREAT,	"FCREAT"	},
#endif
#ifdef FTRUNC
	{ FTRUNC,	"FTRUNC"	},
#endif
#ifdef FEXCL
	{ FEXCL,	"FEXCL"		},
#endif
#ifdef FNBIO
	{ FNBIO,	"FNBIO"		},
#endif
#ifdef FSYNC
	{ FSYNC,	"FSYNC"		},
#endif
#ifdef FNOCTTY
	{ FNOCTTY,	"FNOCTTY"	},
#endif
#ifdef O_SHLOCK
	{ O_SHLOCK,	"O_SHLOCK"	},
#endif
#ifdef O_EXLOCK
	{ O_EXLOCK,	"O_EXLOCK"	},
#endif
	{ 0,		NULL		},
};

#ifdef LINUX

#ifndef AT_FDCWD
# define AT_FDCWD                -100
#endif

/* The fd is an "int", so when decoding x86 on x86_64, we need to force sign
 * extension to get the right value.  We do this by declaring fd as int here.
 */
static void
print_dirfd(struct tcb *tcp, int fd)
{
	if (fd == AT_FDCWD)
		tprintf("AT_FDCWD, ");
	else
	{
		printfd(tcp, fd);
		tprintf(", ");
	}
}
#endif

/*
 * Pity stpcpy() is not standardized...
 */
static char *
str_append(char *dst, const char *src)
{
	while ((*dst = *src++) != '\0')
		dst++;
	return dst;
}

/*
 * low bits of the open(2) flags define access mode,
 * other bits are real flags.
 */
const char *
sprint_open_modes(mode_t flags)
{
	static char outstr[1024];
	char *p;
	char sep = 0;
	const char *str;
	const struct xlat *x;

	p = str_append(outstr, "flags ");
	str = xlookup(open_access_modes, flags & 3);
	if (str) {
		p = str_append(p, str);
		flags &= ~3;
		if (!flags)
			return outstr;
		sep = '|';
	}

	for (x = open_mode_flags; x->str; x++) {
		if ((flags & x->val) == x->val) {
			if (sep)
				*p++ = sep;
			p = str_append(p, x->str);
			flags &= ~x->val;
			if (!flags)
				return outstr;
			sep = '|';
		}
	}
	/* flags is still nonzero */
	if (sep)
		*p++ = sep;
	sprintf(p, "%#x", flags);
	return outstr;
}

void
tprint_open_modes(mode_t flags)
{
	tprintf("%s", sprint_open_modes(flags) + sizeof("flags"));
}

static int
decode_open(struct tcb *tcp, int offset)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", ");
		/* flags */
		tprint_open_modes(tcp->u_arg[offset + 1]);
		if (tcp->u_arg[offset + 1] & O_CREAT) {
			/* mode */
			tprintf(", %#lo", tcp->u_arg[offset + 2]);
		}
	}
	return 0;
}

int
sys_open(struct tcb *tcp)
{
	return decode_open(tcp, 0);
}

#ifdef LINUX
int
sys_openat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_open(tcp, 1);
}
#endif

#ifdef LINUXSPARC
static const struct xlat openmodessol[] = {
	{ 0,		"O_RDWR"	},
	{ 1,		"O_RDONLY"	},
	{ 2,		"O_WRONLY"	},
	{ 0x80,		"O_NONBLOCK"	},
	{ 8,		"O_APPEND"	},
	{ 0x100,	"O_CREAT"	},
	{ 0x200,	"O_TRUNC"	},
	{ 0x400,	"O_EXCL"	},
	{ 0x800,	"O_NOCTTY"	},
	{ 0x10,		"O_SYNC"	},
	{ 0x40,		"O_DSYNC"	},
	{ 0x8000,	"O_RSYNC"	},
	{ 4,		"O_NDELAY"	},
	{ 0x1000,	"O_PRIV"	},
	{ 0,		NULL		},
};

int
solaris_open(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		/* flags */
		printflags(openmodessol, tcp->u_arg[1] + 1, "O_???");
		if (tcp->u_arg[1] & 0x100) {
			/* mode */
			tprintf(", %#lo", tcp->u_arg[2]);
		}
	}
	return 0;
}

#endif

int
sys_creat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", %#lo", tcp->u_arg[1]);
	}
	return 0;
}

static const struct xlat access_flags[] = {
	{ F_OK,		"F_OK",		},
	{ R_OK,		"R_OK"		},
	{ W_OK,		"W_OK"		},
	{ X_OK,		"X_OK"		},
#ifdef EFF_ONLY_OK
	{ EFF_ONLY_OK,	"EFF_ONLY_OK"	},
#endif
#ifdef EX_OK
	{ EX_OK,	"EX_OK"		},
#endif
	{ 0,		NULL		},
};

static int
decode_access(struct tcb *tcp, int offset)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", ");
		printflags(access_flags, tcp->u_arg[offset + 1], "?_OK");
	}
	return 0;
}

int
sys_access(struct tcb *tcp)
{
	return decode_access(tcp, 0);
}

#ifdef LINUX
int
sys_faccessat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_access(tcp, 1);
}
#endif

int
sys_umask(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%#lo", tcp->u_arg[0]);
	}
	return RVAL_OCTAL;
}

static const struct xlat whence[] = {
	{ SEEK_SET,	"SEEK_SET"	},
	{ SEEK_CUR,	"SEEK_CUR"	},
	{ SEEK_END,	"SEEK_END"	},
	{ 0,		NULL		},
};

#ifndef HAVE_LONG_LONG_OFF_T
#if defined (LINUX_MIPSN32)
int
sys_lseek(struct tcb *tcp)
{
	long long offset;
	int _whence;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		offset = tcp->ext_arg[1];
		_whence = tcp->u_arg[2];
		if (_whence == SEEK_SET)
			tprintf("%llu, ", offset);
		else
			tprintf("%lld, ", offset);
		printxval(whence, _whence, "SEEK_???");
	}
	return RVAL_UDECIMAL;
}
#else /* !LINUX_MIPSN32 */
int
sys_lseek(struct tcb *tcp)
{
	off_t offset;
	int _whence;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		offset = tcp->u_arg[1];
		_whence = tcp->u_arg[2];
		if (_whence == SEEK_SET)
			tprintf("%lu, ", offset);
		else
			tprintf("%ld, ", offset);
		printxval(whence, _whence, "SEEK_???");
	}
	return RVAL_UDECIMAL;
}
#endif /* LINUX_MIPSN32 */
#endif

#ifdef LINUX
int
sys_llseek(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		/*
		 * This one call takes explicitly two 32-bit arguments hi, lo,
		 * rather than one 64-bit argument for which LONG_LONG works
		 * appropriate for the native byte order.
		 */
		if (tcp->u_arg[4] == SEEK_SET)
			tprintf(", %llu, ",
				((long long int) tcp->u_arg[1]) << 32 |
				(unsigned long long) (unsigned) tcp->u_arg[2]);
		else
			tprintf(", %lld, ",
				((long long int) tcp->u_arg[1]) << 32 |
				(unsigned long long) (unsigned) tcp->u_arg[2]);
	}
	else {
		long long int off;
		if (syserror(tcp) || umove(tcp, tcp->u_arg[3], &off) < 0)
			tprintf("%#lx, ", tcp->u_arg[3]);
		else
			tprintf("[%llu], ", off);
		printxval(whence, tcp->u_arg[4], "SEEK_???");
	}
	return 0;
}

int
sys_readahead(struct tcb *tcp)
{
	if (entering(tcp)) {
		int argn;
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		argn = printllval(tcp, "%lld", 1);
		tprintf(", %ld", tcp->u_arg[argn]);
	}
	return 0;
}
#endif

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_lseek64(struct tcb *tcp)
{
	if (entering(tcp)) {
		int argn;
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		if (tcp->u_arg[3] == SEEK_SET)
			argn = printllval(tcp, "%llu, ", 1);
		else
			argn = printllval(tcp, "%lld, ", 1);
		printxval(whence, tcp->u_arg[argn], "SEEK_???");
	}
	return RVAL_LUDECIMAL;
}
#endif

#ifndef HAVE_LONG_LONG_OFF_T
int
sys_truncate(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_truncate64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		printllval(tcp, ", %llu", 1);
	}
	return 0;
}
#endif

#ifndef HAVE_LONG_LONG_OFF_T
int
sys_ftruncate(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_ftruncate64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printllval(tcp, "%llu", 1);
	}
	return 0;
}
#endif

/* several stats */

static const struct xlat modetypes[] = {
	{ S_IFREG,	"S_IFREG"	},
	{ S_IFSOCK,	"S_IFSOCK"	},
	{ S_IFIFO,	"S_IFIFO"	},
	{ S_IFLNK,	"S_IFLNK"	},
	{ S_IFDIR,	"S_IFDIR"	},
	{ S_IFBLK,	"S_IFBLK"	},
	{ S_IFCHR,	"S_IFCHR"	},
	{ 0,		NULL		},
};

static const char *
sprintmode(int mode)
{
	static char buf[64];
	const char *s;

	if ((mode & S_IFMT) == 0)
		s = "";
	else if ((s = xlookup(modetypes, mode & S_IFMT)) == NULL) {
		sprintf(buf, "%#o", mode);
		return buf;
	}
	sprintf(buf, "%s%s%s%s", s,
		(mode & S_ISUID) ? "|S_ISUID" : "",
		(mode & S_ISGID) ? "|S_ISGID" : "",
		(mode & S_ISVTX) ? "|S_ISVTX" : "");
	mode &= ~(S_IFMT|S_ISUID|S_ISGID|S_ISVTX);
	if (mode)
		sprintf(buf + strlen(buf), "|%#o", mode);
	s = (*buf == '|') ? buf + 1 : buf;
	return *s ? s : "0";
}

static char *
sprinttime(time_t t)
{
	struct tm *tmp;
	static char buf[32];

	if (t == 0) {
		strcpy(buf, "0");
		return buf;
	}
	if ((tmp = localtime(&t)))
		snprintf(buf, sizeof buf, "%02d/%02d/%02d-%02d:%02d:%02d",
			tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday,
			tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	else
		snprintf(buf, sizeof buf, "%lu", (unsigned long) t);

	return buf;
}

#ifdef LINUXSPARC
typedef struct {
	int     tv_sec;
	int     tv_nsec;
} timestruct_t;

struct solstat {
	unsigned        st_dev;
	int             st_pad1[3];     /* network id */
	unsigned        st_ino;
	unsigned        st_mode;
	unsigned        st_nlink;
	unsigned        st_uid;
	unsigned        st_gid;
	unsigned        st_rdev;
	int             st_pad2[2];
	int             st_size;
	int             st_pad3;        /* st_size, off_t expansion */
	timestruct_t    st_atime;
	timestruct_t    st_mtime;
	timestruct_t    st_ctime;
	int             st_blksize;
	int             st_blocks;
	char            st_fstype[16];
	int             st_pad4[8];     /* expansion area */
};

static void
printstatsol(struct tcb *tcp, long addr)
{
	struct solstat statbuf;

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}
	if (!abbrev(tcp)) {
		tprintf("{st_dev=makedev(%lu, %lu), st_ino=%lu, st_mode=%s, ",
			(unsigned long) ((statbuf.st_dev >> 18) & 0x3fff),
			(unsigned long) (statbuf.st_dev & 0x3ffff),
			(unsigned long) statbuf.st_ino,
			sprintmode(statbuf.st_mode));
		tprintf("st_nlink=%lu, st_uid=%lu, st_gid=%lu, ",
			(unsigned long) statbuf.st_nlink,
			(unsigned long) statbuf.st_uid,
			(unsigned long) statbuf.st_gid);
		tprintf("st_blksize=%lu, ", (unsigned long) statbuf.st_blksize);
		tprintf("st_blocks=%lu, ", (unsigned long) statbuf.st_blocks);
	}
	else
		tprintf("{st_mode=%s, ", sprintmode(statbuf.st_mode));
	switch (statbuf.st_mode & S_IFMT) {
	case S_IFCHR: case S_IFBLK:
		tprintf("st_rdev=makedev(%lu, %lu), ",
			(unsigned long) ((statbuf.st_rdev >> 18) & 0x3fff),
			(unsigned long) (statbuf.st_rdev & 0x3ffff));
		break;
	default:
		tprintf("st_size=%u, ", statbuf.st_size);
		break;
	}
	if (!abbrev(tcp)) {
		tprintf("st_atime=%s, ", sprinttime(statbuf.st_atime.tv_sec));
		tprintf("st_mtime=%s, ", sprinttime(statbuf.st_mtime.tv_sec));
		tprintf("st_ctime=%s}", sprinttime(statbuf.st_ctime.tv_sec));
	}
	else
		tprintf("...}");
}

#if defined (SPARC64)
static void
printstat_sparc64(struct tcb *tcp, long addr)
{
	struct stat_sparc64 statbuf;

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}

	if (!abbrev(tcp)) {
		tprintf("{st_dev=makedev(%lu, %lu), st_ino=%lu, st_mode=%s, ",
			(unsigned long) major(statbuf.st_dev),
			(unsigned long) minor(statbuf.st_dev),
			(unsigned long) statbuf.st_ino,
			sprintmode(statbuf.st_mode));
		tprintf("st_nlink=%lu, st_uid=%lu, st_gid=%lu, ",
			(unsigned long) statbuf.st_nlink,
			(unsigned long) statbuf.st_uid,
			(unsigned long) statbuf.st_gid);
		tprintf("st_blksize=%lu, ",
			(unsigned long) statbuf.st_blksize);
		tprintf("st_blocks=%lu, ",
			(unsigned long) statbuf.st_blocks);
	}
	else
		tprintf("{st_mode=%s, ", sprintmode(statbuf.st_mode));
	switch (statbuf.st_mode & S_IFMT) {
	case S_IFCHR: case S_IFBLK:
		tprintf("st_rdev=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf.st_rdev),
			(unsigned long) minor(statbuf.st_rdev));
		break;
	default:
		tprintf("st_size=%lu, ", statbuf.st_size);
		break;
	}
	if (!abbrev(tcp)) {
		tprintf("st_atime=%s, ", sprinttime(statbuf.st_atime));
		tprintf("st_mtime=%s, ", sprinttime(statbuf.st_mtime));
		tprintf("st_ctime=%s", sprinttime(statbuf.st_ctime));
		tprintf("}");
	}
	else
		tprintf("...}");
}
#endif /* SPARC64 */
#endif /* LINUXSPARC */

#if defined LINUX && defined POWERPC64
struct stat_powerpc32 {
	unsigned int	st_dev;
	unsigned int	st_ino;
	unsigned int	st_mode;
	unsigned short	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	st_rdev;
	unsigned int	st_size;
	unsigned int	st_blksize;
	unsigned int	st_blocks;
	unsigned int	st_atime;
	unsigned int	st_atime_nsec;
	unsigned int	st_mtime;
	unsigned int	st_mtime_nsec;
	unsigned int	st_ctime;
	unsigned int	st_ctime_nsec;
	unsigned int	__unused4;
	unsigned int	__unused5;
};

static void
printstat_powerpc32(struct tcb *tcp, long addr)
{
	struct stat_powerpc32 statbuf;

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}

	if (!abbrev(tcp)) {
		tprintf("{st_dev=makedev(%u, %u), st_ino=%u, st_mode=%s, ",
			major(statbuf.st_dev), minor(statbuf.st_dev),
			statbuf.st_ino,
			sprintmode(statbuf.st_mode));
		tprintf("st_nlink=%u, st_uid=%u, st_gid=%u, ",
			statbuf.st_nlink, statbuf.st_uid, statbuf.st_gid);
		tprintf("st_blksize=%u, ", statbuf.st_blksize);
		tprintf("st_blocks=%u, ", statbuf.st_blocks);
	}
	else
		tprintf("{st_mode=%s, ", sprintmode(statbuf.st_mode));
	switch (statbuf.st_mode & S_IFMT) {
	case S_IFCHR: case S_IFBLK:
		tprintf("st_rdev=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf.st_rdev),
			(unsigned long) minor(statbuf.st_rdev));
		break;
	default:
		tprintf("st_size=%u, ", statbuf.st_size);
		break;
	}
	if (!abbrev(tcp)) {
		tprintf("st_atime=%s, ", sprinttime(statbuf.st_atime));
		tprintf("st_mtime=%s, ", sprinttime(statbuf.st_mtime));
		tprintf("st_ctime=%s", sprinttime(statbuf.st_ctime));
		tprintf("}");
	}
	else
		tprintf("...}");
}
#endif /* LINUX && POWERPC64 */

static const struct xlat fileflags[] = {
#ifdef FREEBSD
	{ UF_NODUMP,	"UF_NODUMP"	},
	{ UF_IMMUTABLE,	"UF_IMMUTABLE"	},
	{ UF_APPEND,	"UF_APPEND"	},
	{ UF_OPAQUE,	"UF_OPAQUE"	},
	{ UF_NOUNLINK,	"UF_NOUNLINK"	},
	{ SF_ARCHIVED,	"SF_ARCHIVED"	},
	{ SF_IMMUTABLE,	"SF_IMMUTABLE"	},
	{ SF_APPEND,	"SF_APPEND"	},
	{ SF_NOUNLINK,	"SF_NOUNLINK"	},
#elif UNIXWARE >= 2
#ifdef 	_S_ISMLD
	{ _S_ISMLD, 	"_S_ISMLD"	},
#endif
#ifdef 	_S_ISMOUNTED
	{ _S_ISMOUNTED, "_S_ISMOUNTED"	},
#endif
#endif
	{ 0,		NULL		},
};

#ifdef FREEBSD
int
sys_chflags(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printflags(fileflags, tcp->u_arg[1], "UF_???");
	}
	return 0;
}

int
sys_fchflags(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printflags(fileflags, tcp->u_arg[1], "UF_???");
	}
	return 0;
}
#endif

#ifndef HAVE_LONG_LONG_OFF_T
static void
realprintstat(struct tcb *tcp, struct stat *statbuf)
{
	if (!abbrev(tcp)) {
		tprintf("{st_dev=makedev(%lu, %lu), st_ino=%lu, st_mode=%s, ",
			(unsigned long) major(statbuf->st_dev),
			(unsigned long) minor(statbuf->st_dev),
			(unsigned long) statbuf->st_ino,
			sprintmode(statbuf->st_mode));
		tprintf("st_nlink=%lu, st_uid=%lu, st_gid=%lu, ",
			(unsigned long) statbuf->st_nlink,
			(unsigned long) statbuf->st_uid,
			(unsigned long) statbuf->st_gid);
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
		tprintf("st_blksize=%lu, ", (unsigned long) statbuf->st_blksize);
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
		tprintf("st_blocks=%lu, ", (unsigned long) statbuf->st_blocks);
#endif
	}
	else
		tprintf("{st_mode=%s, ", sprintmode(statbuf->st_mode));
	switch (statbuf->st_mode & S_IFMT) {
	case S_IFCHR: case S_IFBLK:
#ifdef HAVE_STRUCT_STAT_ST_RDEV
		tprintf("st_rdev=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf->st_rdev),
			(unsigned long) minor(statbuf->st_rdev));
#else /* !HAVE_STRUCT_STAT_ST_RDEV */
		tprintf("st_size=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf->st_size),
			(unsigned long) minor(statbuf->st_size));
#endif /* !HAVE_STRUCT_STAT_ST_RDEV */
		break;
	default:
		tprintf("st_size=%lu, ", (unsigned long) statbuf->st_size);
		break;
	}
	if (!abbrev(tcp)) {
		tprintf("st_atime=%s, ", sprinttime(statbuf->st_atime));
		tprintf("st_mtime=%s, ", sprinttime(statbuf->st_mtime));
		tprintf("st_ctime=%s", sprinttime(statbuf->st_ctime));
#if HAVE_STRUCT_STAT_ST_FLAGS
		tprintf(", st_flags=");
		printflags(fileflags, statbuf->st_flags, "UF_???");
#endif
#if HAVE_STRUCT_STAT_ST_ACLCNT
		tprintf(", st_aclcnt=%d", statbuf->st_aclcnt);
#endif
#if HAVE_STRUCT_STAT_ST_LEVEL
		tprintf(", st_level=%ld", statbuf->st_level);
#endif
#if HAVE_STRUCT_STAT_ST_FSTYPE
		tprintf(", st_fstype=%.*s",
			(int) sizeof statbuf->st_fstype, statbuf->st_fstype);
#endif
#if HAVE_STRUCT_STAT_ST_GEN
		tprintf(", st_gen=%u", statbuf->st_gen);
#endif
		tprintf("}");
	}
	else
		tprintf("...}");
}


static void
printstat(struct tcb *tcp, long addr)
{
	struct stat statbuf;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}

#ifdef LINUXSPARC
	if (current_personality == 1) {
		printstatsol(tcp, addr);
		return;
	}
#ifdef SPARC64
	else if (current_personality == 2) {
		printstat_sparc64(tcp, addr);
		return;
	}
#endif
#endif /* LINUXSPARC */

#if defined LINUX && defined POWERPC64
	if (current_personality == 1) {
		printstat_powerpc32(tcp, addr);
		return;
	}
#endif

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}

	realprintstat(tcp, &statbuf);
}
#endif	/* !HAVE_LONG_LONG_OFF_T */

#if !defined HAVE_STAT64 && defined LINUX && defined X86_64
/*
 * Linux x86_64 has unified `struct stat' but its i386 biarch needs
 * `struct stat64'.  Its <asm-i386/stat.h> definition expects 32-bit `long'.
 * <linux/include/asm-x86_64/ia32.h> is not in the public includes set.
 * __GNUC__ is needed for the required __attribute__ below.
 */
struct stat64 {
	unsigned long long	st_dev;
	unsigned char	__pad0[4];
	unsigned int	__st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned long long	st_rdev;
	unsigned char	__pad3[4];
	long long	st_size;
	unsigned int	st_blksize;
	unsigned long long	st_blocks;
	unsigned int	st_atime;
	unsigned int	st_atime_nsec;
	unsigned int	st_mtime;
	unsigned int	st_mtime_nsec;
	unsigned int	st_ctime;
	unsigned int	st_ctime_nsec;
	unsigned long long	st_ino;
} __attribute__((packed));
# define HAVE_STAT64	1
# define STAT64_SIZE	96
#endif

#ifdef HAVE_STAT64
static void
printstat64(struct tcb *tcp, long addr)
{
	struct stat64 statbuf;

#ifdef STAT64_SIZE
	(void) sizeof(char[sizeof statbuf == STAT64_SIZE ? 1 : -1]);
#endif

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}

#ifdef LINUXSPARC
	if (current_personality == 1) {
		printstatsol(tcp, addr);
		return;
	}
# ifdef SPARC64
	else if (current_personality == 2) {
		printstat_sparc64(tcp, addr);
		return;
	}
# endif
#endif /* LINUXSPARC */

#if defined LINUX && defined X86_64
	if (current_personality == 0) {
		printstat(tcp, addr);
		return;
	}
#endif

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}

	if (!abbrev(tcp)) {
#ifdef HAVE_LONG_LONG
		tprintf("{st_dev=makedev(%lu, %lu), st_ino=%llu, st_mode=%s, ",
#else
		tprintf("{st_dev=makedev(%lu, %lu), st_ino=%lu, st_mode=%s, ",
#endif
			(unsigned long) major(statbuf.st_dev),
			(unsigned long) minor(statbuf.st_dev),
#ifdef HAVE_LONG_LONG
			(unsigned long long) statbuf.st_ino,
#else
			(unsigned long) statbuf.st_ino,
#endif
			sprintmode(statbuf.st_mode));
		tprintf("st_nlink=%lu, st_uid=%lu, st_gid=%lu, ",
			(unsigned long) statbuf.st_nlink,
			(unsigned long) statbuf.st_uid,
			(unsigned long) statbuf.st_gid);
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
		tprintf("st_blksize=%lu, ",
			(unsigned long) statbuf.st_blksize);
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
		tprintf("st_blocks=%lu, ", (unsigned long) statbuf.st_blocks);
#endif /* HAVE_STRUCT_STAT_ST_BLOCKS */
	}
	else
		tprintf("{st_mode=%s, ", sprintmode(statbuf.st_mode));
	switch (statbuf.st_mode & S_IFMT) {
	case S_IFCHR: case S_IFBLK:
#ifdef HAVE_STRUCT_STAT_ST_RDEV
		tprintf("st_rdev=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf.st_rdev),
			(unsigned long) minor(statbuf.st_rdev));
#else /* !HAVE_STRUCT_STAT_ST_RDEV */
		tprintf("st_size=makedev(%lu, %lu), ",
			(unsigned long) major(statbuf.st_size),
			(unsigned long) minor(statbuf.st_size));
#endif /* !HAVE_STRUCT_STAT_ST_RDEV */
		break;
	default:
#ifdef HAVE_LONG_LONG
		tprintf("st_size=%llu, ", (unsigned long long) statbuf.st_size);
#else
		tprintf("st_size=%lu, ", (unsigned long) statbuf.st_size);
#endif
		break;
	}
	if (!abbrev(tcp)) {
		tprintf("st_atime=%s, ", sprinttime(statbuf.st_atime));
		tprintf("st_mtime=%s, ", sprinttime(statbuf.st_mtime));
		tprintf("st_ctime=%s", sprinttime(statbuf.st_ctime));
#if HAVE_STRUCT_STAT_ST_FLAGS
		tprintf(", st_flags=");
		printflags(fileflags, statbuf.st_flags, "UF_???");
#endif
#if HAVE_STRUCT_STAT_ST_ACLCNT
		tprintf(", st_aclcnt=%d", statbuf.st_aclcnt);
#endif
#if HAVE_STRUCT_STAT_ST_LEVEL
		tprintf(", st_level=%ld", statbuf.st_level);
#endif
#if HAVE_STRUCT_STAT_ST_FSTYPE
		tprintf(", st_fstype=%.*s",
			(int) sizeof statbuf.st_fstype, statbuf.st_fstype);
#endif
#if HAVE_STRUCT_STAT_ST_GEN
		tprintf(", st_gen=%u", statbuf.st_gen);
#endif
		tprintf("}");
	}
	else
		tprintf("...}");
}
#endif /* HAVE_STAT64 */

#if defined(LINUX) && defined(HAVE_STRUCT___OLD_KERNEL_STAT)
static void
convertoldstat(const struct __old_kernel_stat *oldbuf, struct stat *newbuf)
{
	newbuf->st_dev = oldbuf->st_dev;
	newbuf->st_ino = oldbuf->st_ino;
	newbuf->st_mode = oldbuf->st_mode;
	newbuf->st_nlink = oldbuf->st_nlink;
	newbuf->st_uid = oldbuf->st_uid;
	newbuf->st_gid = oldbuf->st_gid;
	newbuf->st_rdev = oldbuf->st_rdev;
	newbuf->st_size = oldbuf->st_size;
	newbuf->st_atime = oldbuf->st_atime;
	newbuf->st_mtime = oldbuf->st_mtime;
	newbuf->st_ctime = oldbuf->st_ctime;
	newbuf->st_blksize = 0; /* not supported in old_stat */
	newbuf->st_blocks = 0; /* not supported in old_stat */
}


static void
printoldstat(struct tcb *tcp, long addr)
{
	struct __old_kernel_stat statbuf;
	struct stat newstatbuf;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}

#ifdef LINUXSPARC
	if (current_personality == 1) {
		printstatsol(tcp, addr);
		return;
	}
#endif /* LINUXSPARC */

	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}

	convertoldstat(&statbuf, &newstatbuf);
	realprintstat(tcp, &newstatbuf);
}
#endif /* LINUX && !IA64 && !HPPA && !X86_64 && !S390 && !S390X */

#ifndef HAVE_LONG_LONG_OFF_T
int
sys_stat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif

int
sys_stat64(struct tcb *tcp)
{
#ifdef HAVE_STAT64
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat64(tcp, tcp->u_arg[1]);
	}
	return 0;
#else
	return printargs(tcp);
#endif
}

#ifdef LINUX
static const struct xlat fstatatflags[] = {
#ifndef AT_SYMLINK_NOFOLLOW
# define AT_SYMLINK_NOFOLLOW     0x100
#endif
	{ AT_SYMLINK_NOFOLLOW,	"AT_SYMLINK_NOFOLLOW"	},
	{ 0,			NULL			},
};
#define utimensatflags fstatatflags

int
sys_newfstatat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
	} else {
#ifdef POWERPC64
		if (current_personality == 0)
			printstat(tcp, tcp->u_arg[2]);
		else
			printstat64(tcp, tcp->u_arg[2]);
#elif defined HAVE_STAT64
		printstat64(tcp, tcp->u_arg[2]);
#else
		printstat(tcp, tcp->u_arg[2]);
#endif
		tprintf(", ");
		printflags(fstatatflags, tcp->u_arg[3], "AT_???");
	}
	return 0;
}
#endif

#if defined(LINUX) && defined(HAVE_STRUCT___OLD_KERNEL_STAT)
int
sys_oldstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printoldstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif /* LINUX && HAVE_STRUCT___OLD_KERNEL_STAT */

#ifndef HAVE_LONG_LONG_OFF_T
int
sys_fstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif

int
sys_fstat64(struct tcb *tcp)
{
#ifdef HAVE_STAT64
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat64(tcp, tcp->u_arg[1]);
	}
	return 0;
#else
	return printargs(tcp);
#endif
}

#if defined(LINUX) && defined(HAVE_STRUCT___OLD_KERNEL_STAT)
int
sys_oldfstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printoldstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif /* LINUX && HAVE_STRUCT___OLD_KERNEL_STAT */

#ifndef HAVE_LONG_LONG_OFF_T
int
sys_lstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif

int
sys_lstat64(struct tcb *tcp)
{
#ifdef HAVE_STAT64
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstat64(tcp, tcp->u_arg[1]);
	}
	return 0;
#else
	return printargs(tcp);
#endif
}

#if defined(LINUX) && defined(HAVE_STRUCT___OLD_KERNEL_STAT)
int
sys_oldlstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printoldstat(tcp, tcp->u_arg[1]);
	}
	return 0;
}
#endif /* LINUX && HAVE_STRUCT___OLD_KERNEL_STAT */


#if defined(SVR4) || defined(LINUXSPARC)

int
sys_xstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
	} else {
#ifdef _STAT64_VER
		if (tcp->u_arg[0] == _STAT64_VER)
			printstat64 (tcp, tcp->u_arg[2]);
		else
#endif
		printstat(tcp, tcp->u_arg[2]);
	}
	return 0;
}

int
sys_fxstat(struct tcb *tcp)
{
	if (entering(tcp))
		tprintf("%ld, %ld, ", tcp->u_arg[0], tcp->u_arg[1]);
	else {
#ifdef _STAT64_VER
		if (tcp->u_arg[0] == _STAT64_VER)
			printstat64 (tcp, tcp->u_arg[2]);
		else
#endif
		printstat(tcp, tcp->u_arg[2]);
	}
	return 0;
}

int
sys_lxstat(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
	} else {
#ifdef _STAT64_VER
		if (tcp->u_arg[0] == _STAT64_VER)
			printstat64 (tcp, tcp->u_arg[2]);
		else
#endif
		printstat(tcp, tcp->u_arg[2]);
	}
	return 0;
}

int
sys_xmknod(struct tcb *tcp)
{
	int mode = tcp->u_arg[2];

	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", %s", sprintmode(mode));
		switch (mode & S_IFMT) {
		case S_IFCHR: case S_IFBLK:
#ifdef LINUXSPARC
			tprintf(", makedev(%lu, %lu)",
				(unsigned long) ((tcp->u_arg[3] >> 18) & 0x3fff),
				(unsigned long) (tcp->u_arg[3] & 0x3ffff));
#else
			tprintf(", makedev(%lu, %lu)",
				(unsigned long) major(tcp->u_arg[3]),
				(unsigned long) minor(tcp->u_arg[3]));
#endif
			break;
		default:
			break;
		}
	}
	return 0;
}

#ifdef HAVE_SYS_ACL_H

#include <sys/acl.h>

static const struct xlat aclcmds[] = {
#ifdef SETACL
	{ SETACL,	"SETACL"	},
#endif
#ifdef GETACL
	{ GETACL,	"GETACL"	},
#endif
#ifdef GETACLCNT
	{ GETACLCNT,	"GETACLCNT"	},
#endif
#ifdef ACL_GET
	{ ACL_GET,	"ACL_GET"	},
#endif
#ifdef ACL_SET
	{ ACL_SET,	"ACL_SET"	},
#endif
#ifdef ACL_CNT
	{ ACL_CNT,	"ACL_CNT"	},
#endif
	{ 0,		NULL		},
};

int
sys_acl(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printxval(aclcmds, tcp->u_arg[1], "???ACL???");
		tprintf(", %ld", tcp->u_arg[2]);
		/*
		 * FIXME - dump out the list of aclent_t's pointed to
		 * by "tcp->u_arg[3]" if it's not NULL.
		 */
		if (tcp->u_arg[3])
			tprintf(", %#lx", tcp->u_arg[3]);
		else
			tprintf(", NULL");
	}
	return 0;
}


int
sys_facl(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printxval(aclcmds, tcp->u_arg[1], "???ACL???");
		tprintf(", %ld", tcp->u_arg[2]);
		/*
		 * FIXME - dump out the list of aclent_t's pointed to
		 * by "tcp->u_arg[3]" if it's not NULL.
		 */
		if (tcp->u_arg[3])
			tprintf(", %#lx", tcp->u_arg[3]);
		else
			tprintf(", NULL");
	}
	return 0;
}


static const struct xlat aclipc[] = {
#ifdef IPC_SHM
	{ IPC_SHM,	"IPC_SHM"	},
#endif
#ifdef IPC_SEM
	{ IPC_SEM,	"IPC_SEM"	},
#endif
#ifdef IPC_MSG
	{ IPC_MSG,	"IPC_MSG"	},
#endif
	{ 0,		NULL		},
};


int
sys_aclipc(struct tcb *tcp)
{
	if (entering(tcp)) {
		printxval(aclipc, tcp->u_arg[0], "???IPC???");
		tprintf(", %#lx, ", tcp->u_arg[1]);
		printxval(aclcmds, tcp->u_arg[2], "???ACL???");
		tprintf(", %ld", tcp->u_arg[3]);
		/*
		 * FIXME - dump out the list of aclent_t's pointed to
		 * by "tcp->u_arg[4]" if it's not NULL.
		 */
		if (tcp->u_arg[4])
			tprintf(", %#lx", tcp->u_arg[4]);
		else
			tprintf(", NULL");
	}
	return 0;
}

#endif /* HAVE_SYS_ACL_H */

#endif /* SVR4 || LINUXSPARC */

#ifdef LINUX

static const struct xlat fsmagic[] = {
	{ 0x73757245,	"CODA_SUPER_MAGIC"	},
	{ 0x012ff7b7,	"COH_SUPER_MAGIC"	},
	{ 0x1373,	"DEVFS_SUPER_MAGIC"	},
	{ 0x1cd1,	"DEVPTS_SUPER_MAGIC"	},
	{ 0x414A53,	"EFS_SUPER_MAGIC"	},
	{ 0xef51,	"EXT2_OLD_SUPER_MAGIC"	},
	{ 0xef53,	"EXT2_SUPER_MAGIC"	},
	{ 0x137d,	"EXT_SUPER_MAGIC"	},
	{ 0xf995e849,	"HPFS_SUPER_MAGIC"	},
	{ 0x9660,	"ISOFS_SUPER_MAGIC"	},
	{ 0x137f,	"MINIX_SUPER_MAGIC"	},
	{ 0x138f,	"MINIX_SUPER_MAGIC2"	},
	{ 0x2468,	"MINIX2_SUPER_MAGIC"	},
	{ 0x2478,	"MINIX2_SUPER_MAGIC2"	},
	{ 0x4d44,	"MSDOS_SUPER_MAGIC"	},
	{ 0x564c,	"NCP_SUPER_MAGIC"	},
	{ 0x6969,	"NFS_SUPER_MAGIC"	},
	{ 0x9fa0,	"PROC_SUPER_MAGIC"	},
	{ 0x002f,	"QNX4_SUPER_MAGIC"	},
	{ 0x52654973,	"REISERFS_SUPER_MAGIC"	},
	{ 0x02011994,	"SHMFS_SUPER_MAGIC"	},
	{ 0x517b,	"SMB_SUPER_MAGIC"	},
	{ 0x012ff7b6,	"SYSV2_SUPER_MAGIC"	},
	{ 0x012ff7b5,	"SYSV4_SUPER_MAGIC"	},
	{ 0x00011954,	"UFS_MAGIC"		},
	{ 0x54190100,	"UFS_CIGAM"		},
	{ 0x012ff7b4,	"XENIX_SUPER_MAGIC"	},
	{ 0x012fd16d,	"XIAFS_SUPER_MAGIC"	},
	{ 0x62656572,	"SYSFS_MAGIC"		},
	{ 0,		NULL			},
};

#endif /* LINUX */

#ifndef SVR4

static const char *
sprintfstype(int magic)
{
	static char buf[32];
#ifdef LINUX
	const char *s;

	s = xlookup(fsmagic, magic);
	if (s) {
		sprintf(buf, "\"%s\"", s);
		return buf;
	}
#endif /* LINUX */
	sprintf(buf, "%#x", magic);
	return buf;
}

static void
printstatfs(struct tcb *tcp, long addr)
{
	struct statfs statbuf;

	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}
	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}
#ifdef ALPHA

	tprintf("{f_type=%s, f_fbsize=%u, f_blocks=%u, f_bfree=%u, ",
		sprintfstype(statbuf.f_type),
		statbuf.f_bsize, statbuf.f_blocks, statbuf.f_bfree);
	tprintf("f_bavail=%u, f_files=%u, f_ffree=%u, f_fsid={%d, %d}, f_namelen=%u",
		statbuf.f_bavail,statbuf.f_files, statbuf.f_ffree,
		statbuf.f_fsid.__val[0], statbuf.f_fsid.__val[1],
		statbuf.f_namelen);
#else /* !ALPHA */
	tprintf("{f_type=%s, f_bsize=%lu, f_blocks=%lu, f_bfree=%lu, ",
		sprintfstype(statbuf.f_type),
		(unsigned long)statbuf.f_bsize,
		(unsigned long)statbuf.f_blocks,
		(unsigned long)statbuf.f_bfree);
	tprintf("f_bavail=%lu, f_files=%lu, f_ffree=%lu, f_fsid={%d, %d}",
		(unsigned long)statbuf.f_bavail,
		(unsigned long)statbuf.f_files,
		(unsigned long)statbuf.f_ffree,
		statbuf.f_fsid.__val[0], statbuf.f_fsid.__val[1]);
#ifdef LINUX
	tprintf(", f_namelen=%lu", (unsigned long)statbuf.f_namelen);
#endif /* LINUX */
#endif /* !ALPHA */
#ifdef _STATFS_F_FRSIZE
	tprintf(", f_frsize=%lu", (unsigned long)statbuf.f_frsize);
#endif
	tprintf("}");
}

int
sys_statfs(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstatfs(tcp, tcp->u_arg[1]);
	}
	return 0;
}

int
sys_fstatfs(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstatfs(tcp, tcp->u_arg[1]);
	}
	return 0;
}

#if defined LINUX && defined HAVE_STATFS64
static void
printstatfs64(struct tcb *tcp, long addr)
{
	struct statfs64 statbuf;

	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}
	if (umove(tcp, addr, &statbuf) < 0) {
		tprintf("{...}");
		return;
	}
	tprintf("{f_type=%s, f_bsize=%llu, f_blocks=%llu, f_bfree=%llu, ",
		sprintfstype(statbuf.f_type),
		(unsigned long long)statbuf.f_bsize,
		(unsigned long long)statbuf.f_blocks,
		(unsigned long long)statbuf.f_bfree);
	tprintf("f_bavail=%llu, f_files=%llu, f_ffree=%llu, f_fsid={%d, %d}",
		(unsigned long long)statbuf.f_bavail,
		(unsigned long long)statbuf.f_files,
		(unsigned long long)statbuf.f_ffree,
		statbuf.f_fsid.__val[0], statbuf.f_fsid.__val[1]);
	tprintf(", f_namelen=%lu", (unsigned long)statbuf.f_namelen);
#ifdef _STATFS_F_FRSIZE
	tprintf(", f_frsize=%llu", (unsigned long long)statbuf.f_frsize);
#endif
	tprintf("}");
}

int
sys_statfs64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", %lu, ", tcp->u_arg[1]);
	} else {
		if (tcp->u_arg[1] == sizeof (struct statfs64))
			printstatfs64(tcp, tcp->u_arg[2]);
		else
			tprintf("{???}");
	}
	return 0;
}

int
sys_fstatfs64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", %lu, ", tcp->u_arg[1]);
	} else {
		if (tcp->u_arg[1] == sizeof (struct statfs64))
			printstatfs64(tcp, tcp->u_arg[2]);
		else
			tprintf("{???}");
	}
	return 0;
}
#endif

#if defined(LINUX) && defined(__alpha)

int
osf_statfs(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		printstatfs(tcp, tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

int
osf_fstatfs(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
	} else {
		printstatfs(tcp, tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}
#endif /* LINUX && __alpha */

#endif /* !SVR4 */

#ifdef SUNOS4
int
sys_ustat(struct tcb *tcp)
{
	struct ustat statbuf;

	if (entering(tcp)) {
		tprintf("makedev(%lu, %lu), ",
				(long) major(tcp->u_arg[0]),
				(long) minor(tcp->u_arg[0]));
	}
	else {
		if (syserror(tcp) || !verbose(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else if (umove(tcp, tcp->u_arg[1], &statbuf) < 0)
			tprintf("{...}");
		else {
			tprintf("{f_tfree=%lu, f_tinode=%lu, ",
				statbuf.f_tfree, statbuf.f_tinode);
			tprintf("f_fname=\"%.*s\", ",
				(int) sizeof(statbuf.f_fname),
				statbuf.f_fname);
			tprintf("f_fpack=\"%.*s\"}",
				(int) sizeof(statbuf.f_fpack),
				statbuf.f_fpack);
		}
	}
	return 0;
}
#endif /* SUNOS4 */

int
sys_pivotroot(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printpath(tcp, tcp->u_arg[1]);
	}
	return 0;
}


/* directory */
int
sys_chdir(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
	}
	return 0;
}

static int
decode_mkdir(struct tcb *tcp, int offset)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", %#lo", tcp->u_arg[offset + 1]);
	}
	return 0;
}

int
sys_mkdir(struct tcb *tcp)
{
	return decode_mkdir(tcp, 0);
}

#ifdef LINUX
int
sys_mkdirat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_mkdir(tcp, 1);
}
#endif

int
sys_rmdir(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
	}
	return 0;
}

int
sys_fchdir(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
	}
	return 0;
}

int
sys_chroot(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
	}
	return 0;
}

#if defined(SUNOS4) || defined(SVR4)
int
sys_fchroot(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
	}
	return 0;
}
#endif /* SUNOS4 || SVR4 */

int
sys_link(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printpath(tcp, tcp->u_arg[1]);
	}
	return 0;
}

#ifdef LINUX
int
sys_linkat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
		print_dirfd(tcp, tcp->u_arg[2]);
		printpath(tcp, tcp->u_arg[3]);
		tprintf(", ");
		printfd(tcp, tcp->u_arg[4]);
	}
	return 0;
}
#endif

int
sys_unlink(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
	}
	return 0;
}

#ifdef LINUX
static const struct xlat unlinkatflags[] = {
#ifndef AT_REMOVEDIR
# define AT_REMOVEDIR            0x200
#endif
	{ AT_REMOVEDIR,	"AT_REMOVEDIR"	},
	{ 0,		NULL		},
};

int
sys_unlinkat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
		printflags(unlinkatflags, tcp->u_arg[2], "AT_???");
	}
	return 0;
}
#endif

int
sys_symlink(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printpath(tcp, tcp->u_arg[1]);
	}
	return 0;
}

#ifdef LINUX
int
sys_symlinkat(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		print_dirfd(tcp, tcp->u_arg[1]);
		printpath(tcp, tcp->u_arg[2]);
	}
	return 0;
}
#endif

static int
decode_readlink(struct tcb *tcp, int offset)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", ");
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[offset + 1]);
		else
			printpathn(tcp, tcp->u_arg[offset + 1], tcp->u_rval);
		tprintf(", %lu", tcp->u_arg[offset + 2]);
	}
	return 0;
}

int
sys_readlink(struct tcb *tcp)
{
	return decode_readlink(tcp, 0);
}

#ifdef LINUX
int
sys_readlinkat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_readlink(tcp, 1);
}
#endif

int
sys_rename(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printpath(tcp, tcp->u_arg[1]);
	}
	return 0;
}

#ifdef LINUX
int
sys_renameat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
		print_dirfd(tcp, tcp->u_arg[2]);
		printpath(tcp, tcp->u_arg[3]);
	}
	return 0;
}
#endif

int
sys_chown(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
		printuid(", ", tcp->u_arg[2]);
	}
	return 0;
}

#ifdef LINUX
int
sys_fchownat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		printpath(tcp, tcp->u_arg[1]);
		printuid(", ", tcp->u_arg[2]);
		printuid(", ", tcp->u_arg[3]);
		tprintf(", ");
		printflags(fstatatflags, tcp->u_arg[4], "AT_???");
	}
	return 0;
}
#endif

int
sys_fchown(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		printuid(", ", tcp->u_arg[1]);
		printuid(", ", tcp->u_arg[2]);
	}
	return 0;
}

static int
decode_chmod(struct tcb *tcp, int offset)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", %#lo", tcp->u_arg[offset + 1]);
	}
	return 0;
}

int
sys_chmod(struct tcb *tcp)
{
	return decode_chmod(tcp, 0);
}

#ifdef LINUX
int
sys_fchmodat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_chmod(tcp, 1);
}
#endif

int
sys_fchmod(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", %#lo", tcp->u_arg[1]);
	}
	return 0;
}

#ifdef ALPHA
int
sys_osf_utimes(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printtv_bitness(tcp, tcp->u_arg[1], BITNESS_32,  0);
	}
	return 0;
}
#endif

static int
decode_utimes(struct tcb *tcp, int offset, int special)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", ");
		if (tcp->u_arg[offset + 1] == 0)
			tprintf("NULL");
		else {
			tprintf("{");
			printtv_bitness(tcp, tcp->u_arg[offset + 1],
					BITNESS_CURRENT, special);
			tprintf(", ");
			printtv_bitness(tcp, tcp->u_arg[offset + 1]
					+ sizeof (struct timeval),
					BITNESS_CURRENT, special);
			tprintf("}");
		}
	}
	return 0;
}

int
sys_utimes(struct tcb *tcp)
{
	return decode_utimes(tcp, 0, 0);
}

#ifdef LINUX
int
sys_futimesat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_utimes(tcp, 1, 0);
}

int
sys_utimensat(struct tcb *tcp)
{
	if (entering(tcp)) {
		print_dirfd(tcp, tcp->u_arg[0]);
		decode_utimes(tcp, 1, 1);
		tprintf(", ");
		printflags(utimensatflags, tcp->u_arg[3], "AT_???");
	}
	return 0;
}
#endif

int
sys_utime(struct tcb *tcp)
{
	union {
		long utl[2];
		int uti[2];
	} u;

	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		if (!tcp->u_arg[1])
			tprintf("NULL");
		else if (!verbose(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else if (umoven(tcp, tcp->u_arg[1],
				2 * personality_wordsize[current_personality],
				(char *) &u) < 0)
			tprintf("[?, ?]");
		else if (personality_wordsize[current_personality]
			 == sizeof u.utl[0]) {
			tprintf("[%s,", sprinttime(u.utl[0]));
			tprintf(" %s]", sprinttime(u.utl[1]));
		}
		else if (personality_wordsize[current_personality]
			 == sizeof u.uti[0]) {
			tprintf("[%s,", sprinttime(u.uti[0]));
			tprintf(" %s]", sprinttime(u.uti[1]));
		}
		else
			abort();
	}
	return 0;
}

static int
decode_mknod(struct tcb *tcp, int offset)
{
	int mode = tcp->u_arg[offset + 1];

	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[offset]);
		tprintf(", %s", sprintmode(mode));
		switch (mode & S_IFMT) {
		case S_IFCHR: case S_IFBLK:
#ifdef LINUXSPARC
			if (current_personality == 1)
			tprintf(", makedev(%lu, %lu)",
				(unsigned long) ((tcp->u_arg[offset + 2] >> 18) & 0x3fff),
				(unsigned long) (tcp->u_arg[offset + 2] & 0x3ffff));
			else
#endif
			tprintf(", makedev(%lu, %lu)",
				(unsigned long) major(tcp->u_arg[offset + 2]),
				(unsigned long) minor(tcp->u_arg[offset + 2]));
			break;
		default:
			break;
		}
	}
	return 0;
}

int
sys_mknod(struct tcb *tcp)
{
	return decode_mknod(tcp, 0);
}

#ifdef LINUX
int
sys_mknodat(struct tcb *tcp)
{
	if (entering(tcp))
		print_dirfd(tcp, tcp->u_arg[0]);
	return decode_mknod(tcp, 1);
}
#endif

#ifdef FREEBSD
int
sys_mkfifo(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", %#lo", tcp->u_arg[1]);
	}
	return 0;
}
#endif /* FREEBSD */

int
sys_fsync(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
	}
	return 0;
}

#ifdef LINUX

static void
printdir(struct tcb *tcp, long addr)
{
	struct dirent d;

	if (!verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}
	if (umove(tcp, addr, &d) < 0) {
		tprintf("{...}");
		return;
	}
	tprintf("{d_ino=%ld, ", (unsigned long) d.d_ino);
	tprintf("d_name=");
	printpathn(tcp, (long) ((struct dirent *) addr)->d_name, d.d_reclen);
	tprintf("}");
}

int
sys_readdir(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		if (syserror(tcp) || tcp->u_rval == 0 || !verbose(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printdir(tcp, tcp->u_arg[1]);
		/* Not much point in printing this out, it is always 1. */
		if (tcp->u_arg[2] != 1)
			tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

#endif /* LINUX */

#if defined FREEBSD || defined LINUX
static const struct xlat direnttypes[] = {
	{ DT_UNKNOWN,	"DT_UNKNOWN" 	},
	{ DT_FIFO,	"DT_FIFO" 	},
	{ DT_CHR,	"DT_CHR" 	},
	{ DT_DIR,	"DT_DIR" 	},
	{ DT_BLK,	"DT_BLK" 	},
	{ DT_REG,	"DT_REG" 	},
	{ DT_LNK,	"DT_LNK" 	},
	{ DT_SOCK,	"DT_SOCK" 	},
	{ DT_WHT,	"DT_WHT" 	},
	{ 0,		NULL		},
};

#endif

int
sys_getdents(struct tcb *tcp)
{
	int i, len, dents = 0;
	char *buf;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		return 0;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		return 0;
	}
	len = tcp->u_rval;
	buf = len ? malloc(len) : NULL;
	if (len && !buf) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		fprintf(stderr, "out of memory\n");
		return 0;
	}
	if (umoven(tcp, tcp->u_arg[1], len, buf) < 0) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		free(buf);
		return 0;
	}
	if (!abbrev(tcp))
		tprintf("{");
	for (i = 0; i < len;) {
		struct kernel_dirent *d = (struct kernel_dirent *) &buf[i];
#ifdef LINUX
		if (!abbrev(tcp)) {
			tprintf("%s{d_ino=%lu, d_off=%lu, ",
				i ? " " : "", d->d_ino, d->d_off);
			tprintf("d_reclen=%u, d_name=\"%s\"}",
				d->d_reclen, d->d_name);
		}
#endif /* LINUX */
#ifdef SVR4
		if (!abbrev(tcp)) {
			tprintf("%s{d_ino=%lu, d_off=%lu, ",
				i ? " " : "",
				(unsigned long) d->d_ino,
				(unsigned long) d->d_off);
			tprintf("d_reclen=%u, d_name=\"%s\"}",
				d->d_reclen, d->d_name);
		}
#endif /* SVR4 */
#ifdef SUNOS4
		if (!abbrev(tcp)) {
			tprintf("%s{d_off=%lu, d_fileno=%lu, d_reclen=%u, ",
				i ? " " : "", d->d_off, d->d_fileno,
				d->d_reclen);
			tprintf("d_namlen=%u, d_name=\"%.*s\"}",
				d->d_namlen, d->d_namlen, d->d_name);
		}
#endif /* SUNOS4 */
#ifdef FREEBSD
		if (!abbrev(tcp)) {
			tprintf("%s{d_fileno=%u, d_reclen=%u, d_type=",
				i ? " " : "", d->d_fileno, d->d_reclen);
			printxval(direnttypes, d->d_type, "DT_???");
			tprintf(", d_namlen=%u, d_name=\"%.*s\"}",
				d->d_namlen, d->d_namlen, d->d_name);
		}
#endif /* FREEBSD */
		if (!d->d_reclen) {
			tprintf("/* d_reclen == 0, problem here */");
			break;
		}
		i += d->d_reclen;
		dents++;
	}
	if (!abbrev(tcp))
		tprintf("}");
	else
		tprintf("/* %u entries */", dents);
	tprintf(", %lu", tcp->u_arg[2]);
	free(buf);
	return 0;
}


#if _LFS64_LARGEFILE
int
sys_getdents64(struct tcb *tcp)
{
	int i, len, dents = 0;
	char *buf;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		return 0;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		return 0;
	}
	len = tcp->u_rval;
	buf = len ? malloc(len) : NULL;
	if (len && !buf) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		fprintf(stderr, "out of memory\n");
		return 0;
	}
	if (umoven(tcp, tcp->u_arg[1], len, buf) < 0) {
		tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
		free(buf);
		return 0;
	}
	if (!abbrev(tcp))
		tprintf("{");
	for (i = 0; i < len;) {
		struct dirent64 *d = (struct dirent64 *) &buf[i];
#if defined(LINUX) || defined(SVR4)
		if (!abbrev(tcp)) {
			tprintf("%s{d_ino=%" PRIu64 ", d_off=%" PRId64 ", ",
				i ? " " : "",
				d->d_ino,
				d->d_off);
#ifdef LINUX
			tprintf("d_type=");
			printxval(direnttypes, d->d_type, "DT_???");
			tprintf(", ");
#endif
			tprintf("d_reclen=%u, d_name=\"%s\"}",
				d->d_reclen, d->d_name);
		}
#endif /* LINUX || SVR4 */
#ifdef SUNOS4
		if (!abbrev(tcp)) {
			tprintf("%s{d_off=%lu, d_fileno=%lu, d_reclen=%u, ",
				i ? " " : "", d->d_off, d->d_fileno,
				d->d_reclen);
			tprintf("d_namlen=%u, d_name=\"%.*s\"}",
				d->d_namlen, d->d_namlen, d->d_name);
		}
#endif /* SUNOS4 */
		if (!d->d_reclen) {
			tprintf("/* d_reclen == 0, problem here */");
			break;
		}
		i += d->d_reclen;
		dents++;
	}
	if (!abbrev(tcp))
		tprintf("}");
	else
		tprintf("/* %u entries */", dents);
	tprintf(", %lu", tcp->u_arg[2]);
	free(buf);
	return 0;
}
#endif

#ifdef FREEBSD
int
sys_getdirentries(struct tcb *tcp)
{
	int i, len, dents = 0;
	long basep;
	char *buf;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		return 0;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx, %lu, %#lx", tcp->u_arg[1], tcp->u_arg[2], tcp->u_arg[3]);
		return 0;
	}
	len = tcp->u_rval;
	if ((buf = malloc(len)) == NULL) {
		tprintf("%#lx, %lu, %#lx", tcp->u_arg[1], tcp->u_arg[2], tcp->u_arg[3]);
		fprintf(stderr, "out of memory\n");
		return 0;
	}
	if (umoven(tcp, tcp->u_arg[1], len, buf) < 0) {
		tprintf("%#lx, %lu, %#lx", tcp->u_arg[1], tcp->u_arg[2], tcp->u_arg[3]);
		free(buf);
		return 0;
	}
	if (!abbrev(tcp))
		tprintf("{");
	for (i = 0; i < len;) {
		struct kernel_dirent *d = (struct kernel_dirent *) &buf[i];
		if (!abbrev(tcp)) {
			tprintf("%s{d_fileno=%u, d_reclen=%u, d_type=",
				i ? " " : "", d->d_fileno, d->d_reclen);
			printxval(direnttypes, d->d_type, "DT_???");
			tprintf(", d_namlen=%u, d_name=\"%.*s\"}",
				d->d_namlen, d->d_namlen, d->d_name);
		}
		if (!d->d_reclen) {
			tprintf("/* d_reclen == 0, problem here */");
			break;
		}
		i += d->d_reclen;
		dents++;
	}
	if (!abbrev(tcp))
		tprintf("}");
	else
		tprintf("/* %u entries */", dents);
	free(buf);
	tprintf(", %lu", tcp->u_arg[2]);
	if (umove(tcp, tcp->u_arg[3], &basep) < 0)
		tprintf(", %#lx", tcp->u_arg[3]);
	else
		tprintf(", [%lu]", basep);
	return 0;
}
#endif

#ifdef LINUX
int
sys_getcwd(struct tcb *tcp)
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else
			printpathn(tcp, tcp->u_arg[0], tcp->u_rval - 1);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif /* LINUX */

#ifdef FREEBSD
int
sys___getcwd(struct tcb *tcp)
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[0]);
		else
			printpathn(tcp, tcp->u_arg[0], tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[1]);
	}
	return 0;
}
#endif

#ifdef HAVE_SYS_ASYNCH_H

int
sys_aioread(struct tcb *tcp)
{
	struct aio_result_t res;

	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, %lu, ", tcp->u_arg[2], tcp->u_arg[3]);
		printxval(whence, tcp->u_arg[4], "L_???");
		if (syserror(tcp) || tcp->u_arg[5] == 0
		    || umove(tcp, tcp->u_arg[5], &res) < 0)
			tprintf(", %#lx", tcp->u_arg[5]);
		else
			tprintf(", {aio_return %d aio_errno %d}",
				res.aio_return, res.aio_errno);
	}
	return 0;
}

int
sys_aiowrite(struct tcb *tcp)
{
	struct aio_result_t res;

	if (entering(tcp)) {
		tprintf("%lu, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, %lu, ", tcp->u_arg[2], tcp->u_arg[3]);
		printxval(whence, tcp->u_arg[4], "L_???");
	}
	else {
		if (tcp->u_arg[5] == 0)
			tprintf(", NULL");
		else if (syserror(tcp)
		    || umove(tcp, tcp->u_arg[5], &res) < 0)
			tprintf(", %#lx", tcp->u_arg[5]);
		else
			tprintf(", {aio_return %d aio_errno %d}",
				res.aio_return, res.aio_errno);
	}
	return 0;
}

int
sys_aiowait(struct tcb *tcp)
{
	if (entering(tcp))
		printtv(tcp, tcp->u_arg[0]);
	return 0;
}

int
sys_aiocancel(struct tcb *tcp)
{
	struct aio_result_t res;

	if (exiting(tcp)) {
		if (tcp->u_arg[0] == 0)
			tprintf("NULL");
		else if (syserror(tcp)
		    || umove(tcp, tcp->u_arg[0], &res) < 0)
			tprintf("%#lx", tcp->u_arg[0]);
		else
			tprintf("{aio_return %d aio_errno %d}",
				res.aio_return, res.aio_errno);
	}
	return 0;
}

#endif /* HAVE_SYS_ASYNCH_H */

static const struct xlat xattrflags[] = {
#ifdef XATTR_CREATE
	{ XATTR_CREATE,	 "XATTR_CREATE" },
	{ XATTR_REPLACE, "XATTR_REPLACE" },
#endif
	{ 0,		 NULL }
};

static void
print_xattr_val(struct tcb *tcp, int failed,
		unsigned long arg,
		unsigned long insize,
		unsigned long size)
{
	if (!failed) {
		unsigned long capacity = 4 * size + 1;
		unsigned char *buf = (capacity < size) ? NULL : malloc(capacity);
		if (buf == NULL || /* probably a bogus size argument */
			umoven(tcp, arg, size, (char *) &buf[3 * size]) < 0) {
			failed = 1;
		}
		else {
			unsigned char *out = buf;
			unsigned char *in = &buf[3 * size];
			size_t i;
			for (i = 0; i < size; ++i) {
				if (isprint(in[i]))
					*out++ = in[i];
				else {
#define tohex(n) "0123456789abcdef"[n]
					*out++ = '\\';
					*out++ = 'x';
					*out++ = tohex(in[i] / 16);
					*out++ = tohex(in[i] % 16);
				}
			}
			/* Don't print terminating NUL if there is one.  */
			if (i > 0 && in[i - 1] == '\0')
				out -= 4;
			*out = '\0';
			tprintf(", \"%s\", %ld", buf, insize);
		}
		free(buf);
	}
	if (failed)
		tprintf(", 0x%lx, %ld", arg, insize);
}

int
sys_setxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
		print_xattr_val(tcp, 0, tcp->u_arg[2], tcp->u_arg[3], tcp->u_arg[3]);
		tprintf(", ");
		printflags(xattrflags, tcp->u_arg[4], "XATTR_???");
	}
	return 0;
}

int
sys_fsetxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
		print_xattr_val(tcp, 0, tcp->u_arg[2], tcp->u_arg[3], tcp->u_arg[3]);
		tprintf(", ");
		printflags(xattrflags, tcp->u_arg[4], "XATTR_???");
	}
	return 0;
}

int
sys_getxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
	} else {
		print_xattr_val(tcp, syserror(tcp), tcp->u_arg[2], tcp->u_arg[3],
				tcp->u_rval);
	}
	return 0;
}

int
sys_fgetxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
	} else {
		print_xattr_val(tcp, syserror(tcp), tcp->u_arg[2], tcp->u_arg[3],
				tcp->u_rval);
	}
	return 0;
}

int
sys_listxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
	} else {
		/* XXX Print value in format */
		tprintf(", %p, %lu", (void *) tcp->u_arg[1], tcp->u_arg[2]);
	}
	return 0;
}

int
sys_flistxattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
	} else {
		/* XXX Print value in format */
		tprintf(", %p, %lu", (void *) tcp->u_arg[1], tcp->u_arg[2]);
	}
	return 0;
}

int
sys_removexattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
	}
	return 0;
}

int
sys_fremovexattr(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
	}
	return 0;
}


static const struct xlat advise[] = {
  { POSIX_FADV_NORMAL,		"POSIX_FADV_NORMAL"	},
  { POSIX_FADV_RANDOM,		"POSIX_FADV_RANDOM"	},
  { POSIX_FADV_SEQUENTIAL,	"POSIX_FADV_SEQUENTIAL"	},
  { POSIX_FADV_WILLNEED,	"POSIX_FADV_WILLNEED"	},
  { POSIX_FADV_DONTNEED,	"POSIX_FADV_DONTNEED"	},
  { POSIX_FADV_NOREUSE,		"POSIX_FADV_NOREUSE"	},
  { 0,				NULL			}
};


#ifdef LINUX
int
sys_fadvise64(struct tcb *tcp)
{
	if (entering(tcp)) {
		int argn;
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		argn = printllval(tcp, "%lld", 1);
		tprintf(", %ld, ", tcp->u_arg[argn++]);
		printxval(advise, tcp->u_arg[argn], "POSIX_FADV_???");
	}
	return 0;
}
#endif


int
sys_fadvise64_64(struct tcb *tcp)
{
	if (entering(tcp)) {
		int argn;
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
#if defined ARM || defined POWERPC
		argn = printllval(tcp, "%lld, ", 2);
#else
		argn = printllval(tcp, "%lld, ", 1);
#endif
		argn = printllval(tcp, "%lld, ", argn);
#if defined ARM || defined POWERPC
		printxval(advise, tcp->u_arg[1], "POSIX_FADV_???");
#else
		printxval(advise, tcp->u_arg[argn], "POSIX_FADV_???");
#endif
	}
	return 0;
}

#ifdef LINUX
static const struct xlat inotify_modes[] = {
	{ 0x00000001,	"IN_ACCESS"	},
	{ 0x00000002,	"IN_MODIFY"	},
	{ 0x00000004,	"IN_ATTRIB"	},
	{ 0x00000008,	"IN_CLOSE_WRITE"},
	{ 0x00000010,	"IN_CLOSE_NOWRITE"},
	{ 0x00000020,	"IN_OPEN"	},
	{ 0x00000040,	"IN_MOVED_FROM"	},
	{ 0x00000080,	"IN_MOVED_TO"	},
	{ 0x00000100,	"IN_CREATE"	},
	{ 0x00000200,	"IN_DELETE"	},
	{ 0x00000400,	"IN_DELETE_SELF"},
	{ 0x00000800,	"IN_MOVE_SELF"	},
	{ 0x00002000,	"IN_UNMOUNT"	},
	{ 0x00004000,	"IN_Q_OVERFLOW"	},
	{ 0x00008000,	"IN_IGNORED"	},
	{ 0x01000000,	"IN_ONLYDIR"	},
	{ 0x02000000,	"IN_DONT_FOLLOW"},
	{ 0x20000000,	"IN_MASK_ADD"	},
	{ 0x40000000,	"IN_ISDIR"	},
	{ 0x80000000,	"IN_ONESHOT"	},
	{ 0,		NULL		}
};

static const struct xlat inotify_init_flags[] = {
	{ 0x00000800,	"IN_NONBLOCK"	},
	{ 0x00080000,	"IN_CLOEXEC" 	},
	{ 0,		NULL 		}
};

int
sys_inotify_add_watch(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printpath(tcp, tcp->u_arg[1]);
		tprintf(", ");
		printflags(inotify_modes, tcp->u_arg[2], "IN_???");
	}
	return 0;
}

int
sys_inotify_rm_watch(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", %ld", tcp->u_arg[1]);
	}
	return 0;
}

int
sys_inotify_init1(struct tcb *tcp)
{
	if (entering(tcp))
		printflags(inotify_init_flags, tcp->u_arg[0], "IN_???");
	return 0;
}

int
sys_fallocate(struct tcb *tcp)
{
	if (entering(tcp)) {
		int argn;
		printfd(tcp, tcp->u_arg[0]);		/* fd */
		tprintf(", ");
		tprintf("%#lo, ", tcp->u_arg[1]);	/* mode */
		argn = printllval(tcp, "%llu, ", 2);	/* offset */
		printllval(tcp, "%llu", argn);		/* len */
	}
	return 0;
}
#endif
