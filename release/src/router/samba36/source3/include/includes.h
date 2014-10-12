#ifndef _INCLUDES_H
#define _INCLUDES_H
/* 
   Unix SMB/CIFS implementation.
   Machine customisation and include handling
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) 2002 by Martin Pool <mbp@samba.org>
   
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

#include "../replace/replace.h"

#if _SAMBA_BUILD_ == 4
# undef _SAMBA_BUILD_
# define _SAMBA_BUILD_ 3
#endif

/* make sure we have included the correct config.h */
#ifndef NO_CONFIG_H /* for some tests */
#ifndef CONFIG_H_IS_FROM_SAMBA
#error "make sure you have removed all config.h files from standalone builds!"
#error "the included config.h isn't from samba!"
#endif
#endif /* NO_CONFIG_H */

/* only do the C++ reserved word check when we compile
   to include --with-developer since too many systems
   still have comflicts with their header files (e.g. IRIX 6.4) */

#if !defined(__cplusplus) && defined(DEVELOPER) && defined(__linux__)
#define class #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define private #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define public #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define protected #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define template #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define this #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define new #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define delete #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#define friend #error DONT_USE_CPLUSPLUS_RESERVED_NAMES
#endif

#include "local.h"

#ifdef SUNOS4
/* on SUNOS4 termios.h conflicts with sys/ioctl.h */
#undef HAVE_TERMIOS_H
#endif

#ifdef RELIANTUNIX
/*
 * <unistd.h> has to be included before any other to get
 * large file support on Reliant UNIX. Yes, it's broken :-).
 */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif /* RELIANTUNIX */

#include "system/dir.h"
#include "system/locale.h"
#include "system/time.h"
#include "system/wait.h"

#if defined(HAVE_RPC_RPC_H)
/*
 * Check for AUTH_ERROR define conflict with rpc/rpc.h in prot.h.
 */
#if defined(HAVE_SYS_SECURITY_H) && defined(HAVE_RPC_AUTH_ERROR_CONFLICT)
#undef AUTH_ERROR
#endif
/*
 * HP-UX 11.X has TCP_NODELAY and TCP_MAXSEG defined in <netinet/tcp.h> which
 * was included above.  However <rpc/rpc.h> includes <sys/xti.h> which defines
 * them again without checking if they already exsist.  This generates
 * two "Redefinition of macro" warnings for every single .c file that is
 * compiled.
 */
#if defined(HPUX) && defined(TCP_NODELAY)
#undef TCP_NODELAY
#endif
#if defined(HPUX) && defined(TCP_MAXSEG)
#undef TCP_MAXSEG
#endif
#include <rpc/rpc.h>
#endif

#if defined(HAVE_YP_GET_DEFAULT_DOMAIN) && defined(HAVE_SETNETGRENT) && defined(HAVE_ENDNETGRENT) && defined(HAVE_GETNETGRENT)
#define HAVE_NETGROUP 1
#endif

#if defined (HAVE_NETGROUP)
#if defined(HAVE_RPCSVC_YP_PROT_H)
/*
 * HP-UX 11.X has TCP_NODELAY and TCP_MAXSEG defined in <netinet/tcp.h> which
 * was included above.  However <rpc/rpc.h> includes <sys/xti.h> which defines
 * them again without checking if they already exsist.  This generates
 * two "Redefinition of macro" warnings for every single .c file that is
 * compiled.
 */
#if defined(HPUX) && defined(TCP_NODELAY)
#undef TCP_NODELAY
#endif
#if defined(HPUX) && defined(TCP_MAXSEG)
#undef TCP_MAXSEG
#endif
#include <rpcsvc/yp_prot.h>
#endif
#if defined(HAVE_RPCSVC_YPCLNT_H)
#include <rpcsvc/ypclnt.h>
#endif
#endif /* HAVE_NETGROUP */

#ifndef HAVE_KRB5_H
#undef HAVE_KRB5
#endif

#ifndef HAVE_LDAP_H
#undef HAVE_LDAP
#endif

#if HAVE_SYS_ATTRIBUTES_H
#include <sys/attributes.h>
#endif

#ifndef ENOATTR
#if defined(ENODATA)
#define ENOATTR ENODATA
#else
#define ENOATTR ENOENT
#endif
#endif

/* mutually exclusive (SuSE 8.2) */
#if HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#if HAVE_NETGROUP_H
#include <netgroup.h>
#endif

#if defined(HAVE_AIO_H) && defined(WITH_AIO)
#include <aio.h>
#endif

/* Special macros that are no-ops except when run under Valgrind on
 * x86.  They've moved a little bit from valgrind 1.0.4 to 1.9.4 */
#if HAVE_VALGRIND_MEMCHECK_H
        /* memcheck.h includes valgrind.h */
#include <valgrind/memcheck.h>
#elif HAVE_VALGRIND_H
#include <valgrind.h>
#endif

/* we support ADS if we want it and have krb5 and ldap libs */
#if defined(WITH_ADS) && defined(HAVE_KRB5) && defined(HAVE_LDAP)
#define HAVE_ADS
#endif

/*
 * Define additional missing types
 */
#if defined(AIX)
typedef sig_atomic_t SIG_ATOMIC_T;
#else
typedef sig_atomic_t volatile SIG_ATOMIC_T;
#endif

#ifndef uchar
#define uchar unsigned char
#endif

/*
   Samba needs type definitions for int16, int32, uint16 and uint32.

   Normally these are signed and unsigned 16 and 32 bit integers, but
   they actually only need to be at least 16 and 32 bits
   respectively. Thus if your word size is 8 bytes just defining them
   as signed and unsigned int will work.
*/

#ifndef uint8
#define uint8 uint8_t
#endif

#if !defined(int16) && !defined(HAVE_INT16_FROM_RPC_RPC_H)
#  define int16 int16_t
   /* needed to work around compile issue on HP-UX 11.x */
#  define _INT16	1
#endif

/*
 * Note we duplicate the size tests in the unsigned 
 * case as int16 may be a typedef from rpc/rpc.h
 */


#if !defined(uint16) && !defined(HAVE_UINT16_FROM_RPC_RPC_H)
#  define uint16 uint16_t
#endif

#if !defined(int32) && !defined(HAVE_INT32_FROM_RPC_RPC_H)
#  define int32 int32_t
#  ifndef _INT32
     /* needed to work around compile issue on HP-UX 11.x */
#    define _INT32	1
#  endif
#endif

/*
 * Note we duplicate the size tests in the unsigned 
 * case as int32 may be a typedef from rpc/rpc.h
 */

#if !defined(uint32) && !defined(HAVE_UINT32_FROM_RPC_RPC_H)
#  define uint32 uint32_t
#endif

/*
 * check for 8 byte long long
 */

#if !defined(uint64)
#  define uint64 uint64_t
#endif

#if !defined(int64)
#  define int64 int64_t
#endif


/*
 * Types for devices, inodes and offsets.
 */

#ifndef SMB_DEV_T
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_DEV64_T)
#    define SMB_DEV_T dev64_t
#  else
#    define SMB_DEV_T dev_t
#  endif
#endif

#ifndef LARGE_SMB_DEV_T
#  if (defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_DEV64_T)) || (defined(SIZEOF_DEV_T) && (SIZEOF_DEV_T == 8))
#    define LARGE_SMB_DEV_T 1
#  endif
#endif

#ifdef LARGE_SMB_DEV_T
#define SDEV_T_VAL(p, ofs, v) (SIVAL((p),(ofs),(v)&0xFFFFFFFF), SIVAL((p),(ofs)+4,(v)>>32))
#define DEV_T_VAL(p, ofs) ((SMB_DEV_T)(((uint64_t)(IVAL((p),(ofs))))| (((uint64_t)(IVAL((p),(ofs)+4))) << 32)))
#else 
#define SDEV_T_VAL(p, ofs, v) (SIVAL((p),(ofs),v),SIVAL((p),(ofs)+4,0))
#define DEV_T_VAL(p, ofs) ((SMB_DEV_T)(IVAL((p),(ofs))))
#endif

/*
 * Setup the correctly sized inode type.
 */

#ifndef SMB_INO_T
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_INO64_T)
#    define SMB_INO_T ino64_t
#  else
#    define SMB_INO_T ino_t
#  endif
#endif

#ifndef LARGE_SMB_INO_T
#  if (defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_INO64_T)) || (defined(SIZEOF_INO_T) && (SIZEOF_INO_T == 8))
#    define LARGE_SMB_INO_T 1
#  endif
#endif

#ifdef LARGE_SMB_INO_T
#define SINO_T_VAL(p, ofs, v) SBVAL(p, ofs, v)
#define INO_T_VAL(p, ofs) ((SMB_INO_T)BVAL(p, ofs))
#else 
#define SINO_T_VAL(p, ofs, v) SBVAL(p, ofs, ((uint64_t)(v)) & UINT32_MAX)
#define INO_T_VAL(p, ofs) ((SMB_INO_T)(IVAL((p),(ofs))))
#endif

#ifndef SMB_OFF_T
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T)
#    define SMB_OFF_T off64_t
#  else
#    define SMB_OFF_T off_t
#  endif
#endif

/* TODO: remove this macros */
#define SBIG_UINT(p, ofs, v) SBVAL(p, ofs, v)
#define BIG_UINT(p, ofs) BVAL(p, ofs)
#define IVAL2_TO_SMB_BIG_UINT(p, ofs) BVAL(p, ofs)

/* this should really be a 64 bit type if possible */
typedef uint64_t br_off;

#define SMB_OFF_T_BITS (sizeof(SMB_OFF_T)*8)

/*
 * Set the define that tells us if we can do 64 bit
 * NT SMB calls.
 */

#ifndef LARGE_SMB_OFF_T
#  if (defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T)) || (defined(SIZEOF_OFF_T) && (SIZEOF_OFF_T == 8))
#    define LARGE_SMB_OFF_T 1
#  endif
#endif

#ifdef LARGE_SMB_OFF_T
#define SOFF_T(p, ofs, v) (SIVAL(p,ofs,(v)&0xFFFFFFFF), SIVAL(p,(ofs)+4,(v)>>32))
#define SOFF_T_R(p, ofs, v) (SIVAL(p,(ofs)+4,(v)&0xFFFFFFFF), SIVAL(p,ofs,(v)>>32))
#define IVAL_TO_SMB_OFF_T(buf,off) ((SMB_OFF_T)(( ((uint64_t)(IVAL((buf),(off)))) & ((uint64_t)0xFFFFFFFF) )))
#else 
#define SOFF_T(p, ofs, v) (SIVAL(p,ofs,v),SIVAL(p,(ofs)+4,0))
#define SOFF_T_R(p, ofs, v) (SIVAL(p,(ofs)+4,v),SIVAL(p,ofs,0))
#define IVAL_TO_SMB_OFF_T(buf,off) ((SMB_OFF_T)(( ((uint32)(IVAL((buf),(off)))) & 0xFFFFFFFF )))
#endif

#ifndef HAVE_BLKSIZE_T
/* This is mainly for HP/UX which defines st_blksize as long */
typedef long blksize_t;
#endif

#ifndef HAVE_BLKCNT_T
/* This is mainly for HP/UX which doesn't have blkcnt_t */
typedef long blkcnt_t;
#endif

/*
 * Type for stat structure.
 */

struct stat_ex {
	dev_t		st_ex_dev;
	ino_t		st_ex_ino;
	mode_t		st_ex_mode;
	nlink_t		st_ex_nlink;
	uid_t		st_ex_uid;
	gid_t		st_ex_gid;
	dev_t		st_ex_rdev;
	off_t		st_ex_size;
	struct timespec st_ex_atime;
	struct timespec st_ex_mtime;
	struct timespec st_ex_ctime;
	struct timespec st_ex_btime; /* birthtime */
	/* Is birthtime real, or was it calculated ? */
	bool		st_ex_calculated_birthtime;
	blksize_t	st_ex_blksize;
	blkcnt_t	st_ex_blocks;

	uint32_t	st_ex_flags;
	uint32_t	st_ex_mask;

	/*
	 * Add space for VFS internal extensions. The initial user of this
	 * would be the onefs modules, passing the snapid from the stat calls
	 * to the file_id_create call. Maybe we'll have to expand this later,
	 * but the core of Samba should never look at this field.
	 */
	uint64_t vfs_private;
};

typedef struct stat_ex SMB_STRUCT_STAT;

/*
 * Type for dirent structure.
 */

#ifndef SMB_STRUCT_DIRENT
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_DIRENT64)
#    define SMB_STRUCT_DIRENT struct dirent64
#  else
#    define SMB_STRUCT_DIRENT struct dirent
#  endif
#endif

/*
 * Type for DIR structure.
 */

#ifndef SMB_STRUCT_DIR
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_DIR64)
#    define SMB_STRUCT_DIR DIR64
#  else
#    define SMB_STRUCT_DIR DIR
#  endif
#endif

/*
 * Defines for 64 bit fcntl locks.
 */

#ifndef SMB_STRUCT_FLOCK
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_FLOCK64) && defined(HAVE_OFF64_T)
#    define SMB_STRUCT_FLOCK struct flock64
#  else
#    define SMB_STRUCT_FLOCK struct flock
#  endif
#endif

#ifndef SMB_F_SETLKW
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_FLOCK64) && defined(HAVE_OFF64_T)
#    define SMB_F_SETLKW F_SETLKW64
#  else
#    define SMB_F_SETLKW F_SETLKW
#  endif
#endif

#ifndef SMB_F_SETLK
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_FLOCK64) && defined(HAVE_OFF64_T)
#    define SMB_F_SETLK F_SETLK64
#  else
#    define SMB_F_SETLK F_SETLK
#  endif
#endif

#ifndef SMB_F_GETLK
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_STRUCT_FLOCK64) && defined(HAVE_OFF64_T)
#    define SMB_F_GETLK F_GETLK64
#  else
#    define SMB_F_GETLK F_GETLK
#  endif
#endif

/*
 * Type for aiocb structure.
 */

#ifndef SMB_STRUCT_AIOCB
#  if defined(WITH_AIO)
#    if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64)
#      define SMB_STRUCT_AIOCB struct aiocb64
#    else
#      define SMB_STRUCT_AIOCB struct aiocb
#    endif
#  else
#    define SMB_STRUCT_AIOCB int /* AIO not being used but we still need the define.... */
#  endif
#endif

enum timestamp_set_resolution {
	TIMESTAMP_SET_SECONDS = 0,
	TIMESTAMP_SET_MSEC,
	TIMESTAMP_SET_NT_OR_BETTER
};

/* Our own fstrings */

/*
                  --------------
                 /              \
                /      REST      \
               /        IN        \
              /       PEACE        \
             /                      \
             | The infamous pstring |
             |                      |
             |                      |
             |      7 December      |
             |                      |
             |         2007         |
            *|     *  *  *          | *
   _________)/\\_//(\/(/\)/\//\/\///|_)_______
*/

#ifndef FSTRING_LEN
#define FSTRING_LEN 256
typedef char fstring[FSTRING_LEN];
#endif

/* Lists, trees, caching, database... */
#include "../lib/util/util.h"
#include "../lib/util/util_net.h"
#include "../lib/util/xfile.h"
#include "../lib/util/memory.h"
#include "../lib/util/attr.h"
#include "../lib/util/tsort.h"
#include "../lib/util/dlinklist.h"

#include <talloc.h>

#include "event.h"

#include "../lib/util/data_blob.h"
#include "../lib/util/time.h"
#include "../lib/util/debug.h"
#include "../lib/util/debug_s3.h"

#include "../libcli/util/ntstatus.h"
#include "../libcli/util/error.h"
#include "../lib/util/charset/charset.h"
#include "dynconfig.h"
#include "locking.h"
#include "smb_perfcount.h"
#include "smb.h"
#include "../lib/util/byteorder.h"

#include "module.h"
#include "../lib/util/talloc_stack.h"
#include "../lib/util/smb_threads.h"
#include "../lib/util/smb_threads_internal.h"

/***** prototypes *****/
#ifndef NO_PROTO_H
#include "proto.h"
#endif

/* String routines */

#include "srvstr.h"
#include "safe_string.h"

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifndef SIGRTMIN
#define SIGRTMIN NSIG
#endif

#if defined(HAVE_PUTPRPWNAM) && defined(AUTH_CLEARTEXT_SEG_CHARS)
#define OSF1_ENH_SEC 1
#endif

#if defined(HAVE_CRYPT16) && defined(HAVE_GETAUTHUID)
#define ULTRIX_AUTH 1
#endif

/* yuck, I'd like a better way of doing this */
#define DIRP_SIZE (256 + 32)

/* default socket options. Dave Miller thinks we should default to TCP_NODELAY
   given the socket IO pattern that Samba uses */
#ifdef TCP_NODELAY
#define DEFAULT_SOCKET_OPTIONS "TCP_NODELAY"
#else
#define DEFAULT_SOCKET_OPTIONS ""
#endif

/* dmalloc -- free heap debugger (dmalloc.org).  This should be near
 * the *bottom* of include files so as not to conflict. */
#ifdef ENABLE_DMALLOC
#  include <dmalloc.h>
#endif


#define MAX_SEC_CTX_DEPTH 8    /* Maximum number of security contexts */


#ifdef GLIBC_HACK_FCNTL64
/* this is a gross hack. 64 bit locking is completely screwed up on
   i386 Linux in glibc 2.1.95 (which ships with RedHat 7.0). This hack
   "fixes" the problem with the current 2.4.0test kernels 
*/
#define fcntl fcntl64
#undef F_SETLKW 
#undef F_SETLK 
#define F_SETLK 13
#define F_SETLKW 14
#endif


/* add varargs prototypes with printf checking */
/*PRINTFLIKE2 */
int fdprintf(int , const char *, ...) PRINTF_ATTRIBUTE(2,3);
/*PRINTFLIKE1 */
int d_printf(const char *, ...) PRINTF_ATTRIBUTE(1,2);
/*PRINTFLIKE2 */
int d_fprintf(FILE *f, const char *, ...) PRINTF_ATTRIBUTE(2,3);

/* PRINTFLIKE2 */
void sys_adminlog(int priority, const char *format_str, ...) PRINTF_ATTRIBUTE(2,3);

/* PRINTFLIKE2 */
int fstr_sprintf(fstring s, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

int d_vfprintf(FILE *f, const char *format, va_list ap) PRINTF_ATTRIBUTE(2,0);

int smb_xvasprintf(char **ptr, const char *format, va_list ap) PRINTF_ATTRIBUTE(2,0);

int asprintf_strupper_m(char **strp, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);
char *talloc_asprintf_strupper_m(TALLOC_CTX *t, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/*
 * Veritas File System.  Often in addition to native.
 * Quotas different.
 */
#if defined(HAVE_SYS_FS_VX_QUOTA_H)
#define VXFS_QUOTA
#endif

#ifndef XATTR_CREATE
#define XATTR_CREATE  0x1       /* set value, fail if attr already exists */
#endif

#ifndef XATTR_REPLACE
#define XATTR_REPLACE 0x2       /* set value, fail if attr does not exist */
#endif

#ifdef TRUE
#undef TRUE
#endif
#define TRUE __ERROR__XX__DONT_USE_TRUE

#ifdef FALSE
#undef FALSE
#endif
#define FALSE __ERROR__XX__DONT_USE_FALSE

/* If we have blacklisted mmap() try to avoid using it accidentally by
   undefining the HAVE_MMAP symbol. */

#ifdef MMAP_BLACKLIST
#undef HAVE_MMAP
#endif

#ifndef CONST_DISCARD
#define CONST_DISCARD(type, ptr)      ((type) ((void *) (ptr)))
#endif

void smb_panic( const char *why ) _NORETURN_;
void dump_core(void) _NORETURN_;
void exit_server(const char *const reason) _NORETURN_;
void exit_server_cleanly(const char *const reason) _NORETURN_;
void exit_server_fault(void) _NORETURN_;

/* samba3 doesn't use uwrap yet */
#define uwrap_enabled() 0

#define BASE_RID (0x000003E8L)

#endif /* _INCLUDES_H */
