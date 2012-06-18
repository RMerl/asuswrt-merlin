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

#if !defined(__cplusplus) && defined(DEVELOPER)
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

#ifdef AIX
#define DEFAULT_PRINTING PRINT_AIX
#define PRINTCAP_NAME "/etc/qconfig"
#endif

#ifdef HPUX
#define DEFAULT_PRINTING PRINT_HPUX
#endif

#ifdef QNX
#define DEFAULT_PRINTING PRINT_QNX
#endif

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

#include "system/capability.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "system/glob.h"
#include "system/iconv.h"
#include "system/locale.h"
#include "system/network.h"
#include "system/passwd.h"
#include "system/readline.h"
#include "system/select.h"
#include "system/shmem.h"
#include "system/syslog.h"
#include "system/terminal.h"
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

#if HAVE_LBER_H
#include <lber.h>
#if defined(HPUX) && !defined(_LBER_TYPES_H)
/* Define ber_tag_t and ber_int_t for using
 * HP LDAP-UX Integration products' LDAP libraries.
*/
#ifndef ber_tag_t
typedef unsigned long ber_tag_t;
typedef int ber_int_t;
#endif
#endif /* defined(HPUX) && !defined(_LBER_TYPES_H) */
#ifndef LBER_USE_DER
#define LBER_USE_DER 0x01
#endif
#endif

#if HAVE_LDAP_H
#include <ldap.h>
#ifndef LDAP_CONST
#define LDAP_CONST const
#endif
#ifndef LDAP_OPT_SUCCESS
#define LDAP_OPT_SUCCESS 0
#endif
/* Solaris 8 and maybe other LDAP implementations spell this "..._INPROGRESS": */
#if defined(LDAP_SASL_BIND_INPROGRESS) && !defined(LDAP_SASL_BIND_IN_PROGRESS)
#define LDAP_SASL_BIND_IN_PROGRESS LDAP_SASL_BIND_INPROGRESS
#endif
/* Solaris 8 defines SSL_LDAP_PORT, not LDAPS_PORT and it only does so if
   LDAP_SSL is defined - but SSL is not working. We just want the
   port number! Let's just define LDAPS_PORT correct. */
#if !defined(LDAPS_PORT)
#define LDAPS_PORT 636
#endif
#else
#undef HAVE_LDAP
#endif

#if HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#elif HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#elif HAVE_GSSAPI_H
#include <gssapi.h>
#endif

#if HAVE_COM_ERR_H
#include <com_err.h>
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

#ifdef WITH_MADVISE_PROTECTED
#include <sys/mman.h>
#endif

/* Special macros that are no-ops except when run under Valgrind on
 * x86.  They've moved a little bit from valgrind 1.0.4 to 1.9.4 */
#if HAVE_VALGRIND_MEMCHECK_H
        /* memcheck.h includes valgrind.h */
#include <valgrind/memcheck.h>
#elif HAVE_VALGRIND_H
#include <valgrind.h>
#endif

/* If we have --enable-developer and the valgrind header is present,
 * then we're OK to use it.  Set a macro so this logic can be done only
 * once. */
#if defined(DEVELOPER)
#if (HAVE_VALGRIND_H || HAVE_VALGRIND_VALGRIND_H)
#define VALGRIND
#endif
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
#define SINO_T_VAL(p, ofs, v) (SIVAL((p),(ofs),(v)&0xFFFFFFFF), SIVAL((p),(ofs)+4,(v)>>32))
#define INO_T_VAL(p, ofs) ((SMB_INO_T)(((uint64_t)(IVAL(p,ofs)))| (((uint64_t)(IVAL(p,(ofs)+4))) << 32)))
#else 
#define SINO_T_VAL(p, ofs, v) (SIVAL(p,ofs,v),SIVAL(p,(ofs)+4,0))
#define INO_T_VAL(p, ofs) ((SMB_INO_T)(IVAL((p),(ofs))))
#endif

#ifndef SMB_OFF_T
#  if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T)
#    define SMB_OFF_T off64_t
#  else
#    define SMB_OFF_T off_t
#  endif
#endif

#define SBIG_UINT(p, ofs, v) (SIVAL(p,ofs,(v)&0xFFFFFFFF), SIVAL(p,(ofs)+4,(v)>>32))
#define BIG_UINT(p, ofs) ((((uint64_t) IVAL(p,(ofs)+4))<<32)|IVAL(p,ofs))
#define IVAL2_TO_SMB_BIG_UINT(buf,off) ( (((uint64_t)(IVAL((buf),(off)))) & ((uint64_t)0xFFFFFFFF)) | \
		(( ((uint64_t)(IVAL((buf),(off+4)))) & ((uint64_t)0xFFFFFFFF) ) << 32 ) )


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

#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
	time_t tv_sec;            /* Seconds.  */
	long tv_nsec;           /* Nanoseconds.  */
};
#endif

enum timestamp_set_resolution {
	TIMESTAMP_SET_SECONDS = 0,
	TIMESTAMP_SET_MSEC,
	TIMESTAMP_SET_NT_OR_BETTER
};

#ifdef HAVE_BROKEN_GETGROUPS
#define GID_T int
#else
#define GID_T gid_t
#endif

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 32 /* Guess... */
#endif

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

/* Samba 3 doesn't use iconv_convenience: */
extern void *cmdline_lp_ctx;
struct smb_iconv_convenience *lp_iconv_convenience(void *lp_ctx);

/* Lists, trees, caching, database... */
#include "../lib/util/util.h"
#include "../lib/util/util_net.h"
#include "../lib/util/xfile.h"
#include "../lib/util/memory.h"
#include "../lib/util/attr.h"
#include "intl.h"
#include "../lib/util/dlinklist.h"
#include "tdb.h"
#include "util_tdb.h"

#include "talloc.h"

#include "event.h"
#include "../lib/util/tevent_unix.h"
#include "../lib/util/tevent_ntstatus.h"
#include "../lib/tsocket/tsocket.h"

#include "../lib/util/data_blob.h"
#include "../lib/util/time.h"
#include "../lib/util/asn1.h"

#include "ads.h"
#include "ads_dns.h"
#include "interfaces.h"
#include "trans2.h"
#include "../libcli/util/error.h"
#include "ntioctl.h"
#include "../lib/util/charset/charset.h"
#include "dynconfig.h"
#include "util_getent.h"
#include "debugparse.h"
#include "privileges.h"
#include "messages.h"
#include "locking.h"
#include "smb_perfcount.h"
#include "smb_signing.h"
#include "smb.h"
#include "nameserv.h"
#include "secrets.h"
#include "../lib/util/byteorder.h"
#include "privileges.h"
#include "rpc_misc.h"
#include "rpc_dce.h"
#include "../librpc/gen_ndr/schannel.h"
#include "mapping.h"
#include "passdb.h"
#include "rpc_secdes.h"
#include "../libgpo/gpo.h"
#include "msdfs.h"
#include "rap.h"
#include "../lib/crypto/md5.h"
#include "../lib/crypto/md4.h"
#include "../lib/crypto/arcfour.h"
#include "../lib/crypto/crc32.h"
#include "../lib/crypto/hmacmd5.h"
#include "ntlmssp.h"
#include "auth.h"
#include "ntdomain.h"
#include "reg_objects.h"
#include "reg_db.h"
#include "librpc/gen_ndr/perfcount.h"
#include "librpc/gen_ndr/notify.h"
#include "librpc/gen_ndr/xattr.h"
#include "librpc/gen_ndr/messaging.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "librpc/rpc/dcerpc.h"
#include "nt_printing.h"
#include "idmap.h"
#include "client.h"

#include "session.h"
#include "popt.h"
#include "mangle.h"
#include "module.h"
#include "nsswitch/winbind_client.h"
#include "dbwrap.h"
#include "packet.h"
#include "ctdbd_conn.h"
#include "../lib/util/talloc_stack.h"
#include "memcache.h"
#include "async_smb.h"
#include "../lib/async_req/async_sock.h"
#include "talloc_dict.h"
#include "services.h"
#include "eventlog.h"
#include "../lib/util/smb_threads.h"
#include "../lib/util/smb_threads_internal.h"
#include "tldap.h"
#include "tldap_util.h"

#include "lib/smbconf/smbconf.h"
#include "lib/smbconf/smbconf_init.h"
#include "lib/smbconf/smbconf_reg.h"
#include "lib/smbconf/smbconf_txt.h"

/* Defines for wisXXX functions. */
#define UNI_UPPER    0x1
#define UNI_LOWER    0x2
#define UNI_DIGIT    0x4
#define UNI_XDIGIT   0x8
#define UNI_SPACE    0x10

#include "nsswitch/winbind_nss.h"

/* forward declaration from printing.h to get around 
   header file dependencies */

struct printjob;

/* forward declarations from smbldap.c */

#include "smbldap.h"

/*
 * Reasons for cache flush.
 */

enum flush_reason_enum {
    SEEK_FLUSH,
    READ_FLUSH,
    WRITE_FLUSH,
    READRAW_FLUSH,
    OPLOCK_RELEASE_FLUSH,
    CLOSE_FLUSH,
    SYNC_FLUSH,
    SIZECHANGE_FLUSH,
    /* NUM_FLUSH_REASONS must remain the last value in the enumeration. */
    NUM_FLUSH_REASONS};

#include "nss_info.h"
#include "modules/nfs4_acls.h"
#include "nsswitch/libwbclient/wbclient.h"

/***** prototypes *****/
#ifndef NO_PROTO_H
#include "proto.h"
#endif
#include "libcli/security/secace.h"
#include "libcli/security/secacl.h"
#include "libcli/security/security_descriptor.h"

#if defined(HAVE_POSIX_ACLS)
#include "modules/vfs_posixacl.h"
#endif

#if defined(HAVE_TRU64_ACLS)
#include "modules/vfs_tru64acl.h"
#endif

#if defined(HAVE_SOLARIS_ACLS) || defined(HAVE_UNIXWARE_ACLS)
#include "modules/vfs_solarisacl.h"
#endif

#if defined(HAVE_HPUX_ACLS)
#include "modules/vfs_hpuxacl.h"
#endif

#if defined(HAVE_IRIX_ACLS)
#include "modules/vfs_irixacl.h"
#endif

#ifdef HAVE_LDAP
#include "ads_protos.h"
#endif

/* We need this after proto.h to reference GetTimeOfDay(). */
#include "smbprofile.h"

/* String routines */

#include "srvstr.h"
#include "safe_string.h"

/* prototypes from lib/util_transfer_file.c */
#include "transfer_file.h"

#ifndef DEFAULT_PRINTING
#ifdef HAVE_CUPS
#define DEFAULT_PRINTING PRINT_CUPS
#define PRINTCAP_NAME "cups"
#elif defined(SYSV)
#define DEFAULT_PRINTING PRINT_SYSV
#define PRINTCAP_NAME "lpstat"
#else
#define DEFAULT_PRINTING PRINT_BSD
#define PRINTCAP_NAME "/etc/printcap"
#endif
#endif

#ifndef PRINTCAP_NAME
#define PRINTCAP_NAME "/etc/printcap"
#endif

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifndef SIGRTMIN
#define SIGRTMIN NSIG
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#if defined(HAVE_PUTPRPWNAM) && defined(AUTH_CLEARTEXT_SEG_CHARS)
#define OSF1_ENH_SEC 1
#endif

#ifndef ALLOW_CHANGE_PASSWORD
#if (defined(HAVE_TERMIOS_H) && defined(HAVE_DUP2) && defined(HAVE_SETSID))
#define ALLOW_CHANGE_PASSWORD 1
#endif
#endif

/* what is the longest significant password available on your system? 
 Knowing this speeds up password searches a lot */
#ifndef PASSWORD_LENGTH
#define PASSWORD_LENGTH 8
#endif

#ifndef HAVE_PIPE
#define SYNC_DNS 1
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


#if HAVE_KERNEL_SHARE_MODES
#ifndef LOCK_MAND 
#define LOCK_MAND	32	/* This is a mandatory flock */
#define LOCK_READ	64	/* ... Which allows concurrent read operations */
#define LOCK_WRITE	128	/* ... Which allows concurrent write operations */
#define LOCK_RW		192	/* ... Which allows concurrent read & write ops */
#endif
#endif

extern int DEBUGLEVEL;

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


/* needed for some systems without iconv. Doesn't really matter
   what error code we use */
#ifndef EILSEQ
#define EILSEQ EIO
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

#ifdef HAVE_LDAP

/* function declarations not included in proto.h */
LDAP *ldap_open_with_timeout(const char *server, int port, unsigned int to);

#endif	/* HAVE_LDAP */

#if defined(HAVE_LINUX_READAHEAD) && ! defined(HAVE_READAHEAD_DECL)
ssize_t readahead(int fd, off64_t offset, size_t count);
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

#ifdef HAVE_LIBNSCD
#include "libnscd.h"
#endif

#if defined(HAVE_IPV6)
void in6_addr_to_sockaddr_storage(struct sockaddr_storage *ss,
				  struct in6_addr ip);
#endif

/* samba3 doesn't use uwrap yet */
#define uwrap_enabled() 0

#endif /* _INCLUDES_H */
