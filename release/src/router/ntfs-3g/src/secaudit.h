/*
 *		General declarations for secaudit
 *
 *	These declarations are organized to enable code sharing with ntfs-3g
 *	library, but should only be used to build tools runnable both
 *	on Linux (dynamic linking) and Windows (static linking)
 *
 * Copyright (c) 2007-2009 Jean-Pierre Andre
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *		General parameters which may have to be adapted to needs
 */

#define SELFTESTS 1 /* include code for self-testing */
#define POSIXACLS 0 /* include code for processing Posix ACLs */
#define NOREVBOM 0 /* temporary */

#define OWNERFROMACL 1 /* must match option in security.c */

#define MAXATTRSZ 65536 /* Max sec attr size (16448 met for WinXP) */
#define MAXSECURID 262144
#define SECBLKSZ 8
#define MAXFILENAME 4096
#define FORCEMASK 0 /* Special (dangerous) option -m to force a mask */
#define MAXLINE 80 /* maximum processed size of a line */
#define BUFSZ 1024		/* buffer size to read mapping file */
#define LINESZ 120              /* maximum useful size of a mapping line */

/*
 *		     Definitions for Linux
 *	Use explicit or implicit dynamic linking
 */

#ifdef HAVE_CONFIG_H
#undef POSIXACLS   /* override default by configure option */
#define USESTUBS 1 /* API stubs generated at link time */
#else
#define USESTUBS 0 /* direct calls to API, based on following definitions */
#define ENVNTFS3G "NTFS3G"
#define LIBFILE64 "/lib64/libntfs-3g.so.4921"
#define LIBFILE "/lib/libntfs-3g.so.4921"
#endif

#define MAPDIR ".NTFS-3G"
#define MAPFILE "UserMapping"
#define MAGIC_API 0x09042009

#ifndef _NTFS_ENDIANS_H

typedef char s8;
typedef short s16;
typedef long long s64;
typedef unsigned char u8;
typedef unsigned short le16, be16, u16;
typedef unsigned long long u64;
#ifdef STSC
typedef long s32;
typedef unsigned long le32, be32, u32;
#else
typedef int s32;
typedef unsigned int le32, be32, u32;
#endif

#ifdef STSC
#define endian_rev16(x) ((((x) & 255L) << 8) + (((x) >> 8) & 255L))
#define endian_rev32(x) ((((x) & 255L) << 24) + (((x) & 0xff00L) << 8) \
		       + (((x) >> 8) & 0xff00L) + (((x) >> 24) & 255L))
#else
#define endian_rev16(x) ((((x) & 255) << 8) + (((x) >> 8) & 255))
#define endian_rev32(x) ((((x) & 255) << 24) + (((x) & 0xff00) << 8) \
		       + (((x) >> 8) & 0xff00) + (((x) >> 24) & 255))
#endif
#define endian_rev64(x) ((((x) & 255LL) << 56) + (((x) & 0xff00LL) << 40) \
		       + (((x) & 0xff0000LL) << 24) + (((x) & 0xff000000LL) << 8) \
		       + (((x) >> 8) & 0xff000000LL) + (((x) >> 24) & 0xff0000LL) \
		       + (((x) >> 40) & 0xff00LL) + (((x) >> 56) & 255LL))

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define cpu_to_be16(x) endian_rev16(x)
#define cpu_to_be32(x) endian_rev32(x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)

#else

#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
#define cpu_to_le16(x) endian_rev16(x)
#define cpu_to_le32(x) endian_rev32(x)
#define cpu_to_le64(x) endian_rev64(x)
#define le16_to_cpu(x) endian_rev16(x)
#define le32_to_cpu(x) endian_rev32(x)
#define le64_to_cpu(x) endian_rev64(x)

#endif

#define const_le16_to_cpu(x) le16_to_cpu(x)
#define const_cpu_to_le16(x) cpu_to_le16(x)
#define const_cpu_to_le32(x) cpu_to_le32(x)
#define const_cpu_to_be16(x) cpu_to_be16(x)
#define const_cpu_to_be32(x) cpu_to_be32(x)

#endif /* _NTFS_ENDIANS_H */

#ifndef FALSE
enum { FALSE, TRUE } ;
#endif /* FALSE */

#ifdef WIN32

typedef unsigned short uid_t;
typedef unsigned short gid_t;

#define UNICODE(c) ((unsigned short)(c))

#define __attribute__(x)

#else

#ifndef BOOL
typedef int BOOL;   /* Already defined in windows.h */
#endif /* BOOL */

#ifdef STSC

#define ENOTSUP 95

#endif /* STSC */

typedef u32 DWORD; /* must be 32 bits whatever the platform */
typedef DWORD *LPDWORD;

#define MS_NONE 0    /* no flag for mounting the device */
#define MS_RDONLY 1  /* flag for mounting the device read-only */

#endif /* WIN32 */

#if defined(WIN32) | defined(STSC)

/*
 *	On non-Linux computers, there is no mount and the user mapping
 *	if fetched from a real file (or a dummy one for self tests)
 */

#define NTFS_FIND_USID(map,uid,buf) ntfs_find_usid(map,uid,buf)
#define NTFS_FIND_GSID(map,gid,buf) ntfs_find_gsid(map,gid,buf)
#define NTFS_FIND_USER(map,usid) ntfs_find_user(map,usid)
#define NTFS_FIND_GROUP(map,gsid) ntfs_find_group(map,gsid)

#else

/*
 *	On Linux computers, there is a mount and the user mapping
 *	if either obtained through the mount process or fetched
 *	from a dummy file for self-tests
 */

#define NTFS_FIND_USID(map,uid,buf) (mappingtype != MAPEXTERN ? \
		ntfs_find_usid(map,uid,buf) : relay_find_usid(map,uid,buf))
#define NTFS_FIND_GSID(map,gid,buf) (mappingtype != MAPEXTERN ? \
		ntfs_find_gsid(map,gid,buf) : relay_find_gsid(map,gid,buf))
#define NTFS_FIND_USER(map,usid) (mappingtype != MAPEXTERN ? \
		ntfs_find_user(map,usid) : relay_find_user(map,usid))
#define NTFS_FIND_GROUP(map,gsid) (mappingtype != MAPEXTERN ? \
		ntfs_find_group(map,gsid) : relay_find_group(map,gsid))

#endif

/*
 *		A few name hijackings or definitions
 *	needed for using code from ntfs-3g
 */

#ifdef WIN32
#define ACL MY_ACL
#define SID MY_SID
#define ACCESS_ALLOWED_ACE MY_ACCESS_ALLOWED_ACE
#define ACCESS_DENIED_ACE MY_ACCESS_DENIED_ACE
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define IO_REPARSE_TAG_MOUNT_POINT 0xa0000003
#define IO_REPARSE_TAG_SYMLINK     0xa000000c
#else
#define SE_OWNER_DEFAULTED  const_cpu_to_le16(1)
#define SE_GROUP_DEFAULTED  const_cpu_to_le16(2)
#define SE_DACL_PRESENT     const_cpu_to_le16(4)
#define SE_SACL_PRESENT     const_cpu_to_le16(0x10)
#define SE_DACL_DEFAULTED   const_cpu_to_le16(8)
#define SE_SELF_RELATIVE    const_cpu_to_le16(0x8000)
#define SID_REVISION 1
#endif /* WIN32 */
#define SE_DACL_PROTECTED   const_cpu_to_le16(0x1000)
#define SE_SACL_PROTECTED   const_cpu_to_le16(0x2000)
#define SE_DACL_AUTO_INHERITED   const_cpu_to_le16(0x400)
#define SE_SACL_AUTO_INHERITED   const_cpu_to_le16(0x800)
#define SE_DACL_AUTO_INHERIT_REQ   cpu_to_le16(0x100)
#define SE_SACL_AUTO_INHERIT_REQ   cpu_to_le16(0x200)

typedef le16 ntfschar;

typedef struct {
	le32 a;
	le16 b,c;
	struct {
		le16 m,n,o,p,  q,r,s,t;
	} ;
} GUID;

#define ntfs_log_error(args...) do { printf("** " args); if (!isatty(1)) fprintf(stderr,args); } while(0)

/*
 *		Struct to hold the input mapping file
 *	(private to this module)
 */

struct MAPLIST {
	struct MAPLIST *next;
	char *uidstr;		/* uid text from the same record */
	char *gidstr;		/* gid text from the same record */
	char *sidstr;		/* sid text from the same record */
	char maptext[LINESZ + 1];
};

/*
 *		A few dummy declarations needed for using code from security.c
 */

#define MFT_RECORD_IS_DIRECTORY const_cpu_to_le16(1)

struct SECURITY_DATA {
	u64 offset;
	char *attr;
	u32 hash;
	u32 length;
	unsigned int filecount:16;
	unsigned int mode:12;
	unsigned int flags:4;
} ;

		/* default security sub-authorities */
enum {
	DEFSECAUTH1 = -1153374643, /* 3141592653 */
	DEFSECAUTH2 = 589793238,
	DEFSECAUTH3 = 462843383,
	DEFSECBASE = 10000
};

#define OWNERID 1016
#define GROUPID 513


#define INSDS1 1
#define INSDS2 2
#define INSII 4
#define INSDH 8

#ifdef WIN32

typedef enum { RECSHOW, RECSET, RECSETPOSIX } RECURSE; 

#endif

/*
 *		A type large enough to hold any SID
 */

typedef char BIGSID[40];

/*
 *		Declarations for memory allocation checks
 */

struct CHKALLOC
   {
   struct CHKALLOC *next;
   void *alloc;
   const char *file;
   int line;
   size_t size;
   } ;

#if defined(WIN32) | defined(STSC)

#define S_ISVTX 01000
#define S_ISGID 02000
#define S_ISUID 04000
#define S_IXUSR 0100
#define S_IWUSR 0200
#define S_IRUSR 0400
#define S_IXGRP 010
#define S_IWGRP 020
#define S_IRGRP 040
#define S_IXOTH 001
#define S_IWOTH 002
#define S_IROTH 004

#endif

#ifdef WIN32
#else
/*
 *
 *   See http://msdn2.microsoft.com/en-us/library/aa379649.aspx
 */

typedef enum {
	DACL_SECURITY_INFORMATION = 4, // The DACL of the object is being referenced.
	SACL_SECURITY_INFORMATION = 8, // The SACL of the object is being referenced.
	LABEL_SECURITY_INFORMATION = 8, // The mandatory integrity label is being referenced.
	GROUP_SECURITY_INFORMATION = 2, // The primary group identifier of the object is being referenced.
	OWNER_SECURITY_INFORMATION = 1, // The owner identifier of the object is being referenced.
} SECURITY_INFORMATION;

#define STANDARD_RIGHTS_READ	  cpu_to_le32(0x20000)
#define STANDARD_RIGHTS_WRITE	  cpu_to_le32(0x20000)
#define STANDARD_RIGHTS_EXECUTE   cpu_to_le32(0x20000)
#define STANDARD_RIGHTS_REQUIRED  cpu_to_le32(0xf0000)

#endif

typedef struct SECHEAD {
	s8 revision;
	s8 alignment;
	le16 control;
	le32 owner;
	le32 group;
	le32 sacl;
	le32 dacl;
} SECURITY_DESCRIPTOR_RELATIVE;

typedef struct ACL {
	s8 revision;
	s8 alignment1;
	le16 size;
	le16 ace_count;
	le16 alignment2;
} ACL;

typedef struct {
	union {
		struct {
			unsigned char revision;
			unsigned char sub_authority_count;
		} ;
		struct {
			/* evade an alignment problem when a 4 byte field  */
			/* in a struct implies alignment of the struct */
			le16 dummy;
			be16 high_part;
			be32 low_part;
		} identifier_authority;
	} ;
	le32 sub_authority[1];
} SID;

typedef u8 ACE_FLAGS;

typedef struct ACE {
	u8 type;
	u8 flags;
	le16 size;
	le32 mask;
	SID sid;
} ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE;


/*
 *          item in the mapping list
 */

struct MAPPING {
	struct MAPPING *next;
	int xid;		/* linux id : uid or gid */
	SID *sid;		/* Windows id : usid or gsid */
	int grcnt;		/* group count (for users only) */
	gid_t *groups;		/* groups which the user is member of */
};

/*
 *		       Posix ACL structures
 */

struct POSIX_ACE {
	u16 tag;
	u16 perms;
	s32 id;
} ;

struct POSIX_ACL {
	u8 version;
	u8 flags;
	u16 filler;
	struct POSIX_ACE ace[0];
} ;

struct POSIX_SECURITY {
	mode_t mode;
	int acccnt;
	int defcnt;
	int firstdef;
	u16 tagsset;
	struct POSIX_ACL acl;
} ;

/*
 *		Posix tags, cpu-endian 16 bits
 */

enum {
	POSIX_ACL_USER_OBJ =	1,
	POSIX_ACL_USER =	2,
	POSIX_ACL_GROUP_OBJ =	4,
	POSIX_ACL_GROUP =	8,
	POSIX_ACL_MASK =	16,
	POSIX_ACL_OTHER =	32,
	POSIX_ACL_SPECIAL =	64  /* internal use only */
} ;

/*
 *		Posix permissions, cpu-endian 16 bits
 */

enum {
	POSIX_PERM_X =		1,
	POSIX_PERM_W =		2,
	POSIX_PERM_R =		4,
	POSIX_PERM_DENIAL =	64 /* internal use only */
} ;

#define POSIX_VERSION 2

/*
 *		 A few definitions adapted from winnt.h
 *	    (Windows version uses actual definitions from winnt.h, which are
 *	     not compatible with code from security.c on a big-endian computer)
 */

#ifndef WIN32

#define DELETE				 cpu_to_le32(0x00010000L)
#define READ_CONTROL			 cpu_to_le32(0x00020000L)
#define WRITE_DAC			 cpu_to_le32(0x00040000L)
#define WRITE_OWNER			 cpu_to_le32(0x00080000L)
#define SYNCHRONIZE			 cpu_to_le32(0x00100000L)


#define FILE_READ_DATA		  cpu_to_le32( 0x0001 )    // file & pipe
#define FILE_LIST_DIRECTORY	  cpu_to_le32( 0x0001 )    // directory

#define FILE_WRITE_DATA 	  cpu_to_le32( 0x0002 )    // file & pipe
#define FILE_ADD_FILE		  cpu_to_le32( 0x0002 )    // directory

#define FILE_APPEND_DATA	  cpu_to_le32( 0x0004 )    // file
#define FILE_ADD_SUBDIRECTORY	  cpu_to_le32( 0x0004 )    // directory
#define FILE_CREATE_PIPE_INSTANCE cpu_to_le32( 0x0004 )    // named pipe


#define FILE_READ_EA		  cpu_to_le32( 0x0008 )    // file & directory

#define FILE_WRITE_EA		  cpu_to_le32( 0x0010 )    // file & directory

#define FILE_EXECUTE		  cpu_to_le32( 0x0020 )    // file
#define FILE_TRAVERSE		  cpu_to_le32( 0x0020 )    // directory

#define FILE_DELETE_CHILD	  cpu_to_le32( 0x0040 )    // directory

#define FILE_READ_ATTRIBUTES	  cpu_to_le32( 0x0080 )    // all

#define FILE_WRITE_ATTRIBUTES	  cpu_to_le32( 0x0100 )    // all

#define FILE_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
				 cpu_to_le32(0x1FF))

#define FILE_GENERIC_READ	  (STANDARD_RIGHTS_READ     |\
				   FILE_READ_DATA	    |\
				   FILE_READ_ATTRIBUTES     |\
				   FILE_READ_EA 	    |\
				   SYNCHRONIZE)


#define FILE_GENERIC_WRITE	  (STANDARD_RIGHTS_WRITE    |\
				   FILE_WRITE_DATA	    |\
				   FILE_WRITE_ATTRIBUTES    |\
				   FILE_WRITE_EA	    |\
				   FILE_APPEND_DATA	    |\
				   SYNCHRONIZE)


#define FILE_GENERIC_EXECUTE	  (STANDARD_RIGHTS_EXECUTE  |\
				   FILE_READ_ATTRIBUTES     |\
				   FILE_EXECUTE 	    |\
				   SYNCHRONIZE)

#define GENERIC_READ			 cpu_to_le32(0x80000000L)
#define GENERIC_WRITE			 cpu_to_le32(0x40000000L)
#define GENERIC_EXECUTE 		 cpu_to_le32(0x20000000L)
#define GENERIC_ALL	 		 cpu_to_le32(0x10000000L)


#define OBJECT_INHERIT_ACE		  (0x1)
#define CONTAINER_INHERIT_ACE		  (0x2)
#define NO_PROPAGATE_INHERIT_ACE	  (0x4)
#define INHERIT_ONLY_ACE		  (0x8)
#define VALID_INHERIT_FLAGS		  (0xF)

/*
 *	     Other useful definitions
 */

#define ACL_REVISION 2
#define ACCESS_ALLOWED_ACE_TYPE 0
#define ACCESS_DENIED_ACE_TYPE 1
#define SECURITY_DESCRIPTOR_REVISION 1

#endif /* !WIN32 */

#ifndef ACL_REVISION_DS	/* not always defined in <windows.h> */
#define ACL_REVISION_DS 4
#endif

/*
 *		Matching of ntfs permissions to Linux permissions
 *	these constants are adapted to endianness
 *	when setting, set them all
 *	when checking, check one is present
 */

          /* flags which are set to mean exec, write or read */

#define FILE_READ (FILE_READ_DATA)
#define FILE_WRITE (FILE_WRITE_DATA | FILE_APPEND_DATA \
		| READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)
#define FILE_EXEC (FILE_EXECUTE)
#define DIR_READ FILE_LIST_DIRECTORY
#define DIR_WRITE (FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD \
	 	| READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)
#define DIR_EXEC (FILE_TRAVERSE)

          /* flags tested for meaning exec, write or read */
	  /* tests for write allow for interpretation of a sticky bit */

#define FILE_GREAD (FILE_READ_DATA | GENERIC_READ)
#define FILE_GWRITE (FILE_WRITE_DATA | FILE_APPEND_DATA | GENERIC_WRITE)
#define FILE_GEXEC (FILE_EXECUTE | GENERIC_EXECUTE)
#define DIR_GREAD (FILE_LIST_DIRECTORY | GENERIC_READ)
#define DIR_GWRITE (FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | GENERIC_WRITE)
#define DIR_GEXEC (FILE_TRAVERSE | GENERIC_EXECUTE)

	/* standard owner (and administrator) rights */

#define OWNER_RIGHTS (DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER \
			| SYNCHRONIZE \
			| FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES \
			| FILE_READ_EA | FILE_WRITE_EA)

	/* standard world rights */

#define WORLD_RIGHTS (READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_EA \
			| SYNCHRONIZE)

          /* inheritance flags for files and directories */

#define FILE_INHERITANCE NO_PROPAGATE_INHERIT_ACE
#define DIR_INHERITANCE (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)

/*
 *		To identify NTFS ACL meaning Posix ACL granted to root
 *	we use rights always granted to anybody, so they have no impact
 *	either on Windows or on Linux.
 */

#define ROOT_OWNER_UNMARK SYNCHRONIZE	/* ACL granted to root as owner */
#define ROOT_GROUP_UNMARK FILE_READ_EA	/* ACL granted to root as group */


struct SII {		/* this is an image of an $SII index entry */
	le16 offs;
	le16 size;
	le32 fill1;
	le16 indexsz;
	le16 indexksz;
	le16 flags;
	le16 fill2;
	le32 keysecurid;

	/* did not find official description for the following */
	le32 hash;
	le32 securid;
	le32 dataoffsl;	/* documented as badly aligned */
	le32 dataoffsh;
	le32 datasize;
} ;

struct SDH {		/* this is an image of an $SDH index entry */
	le16 offs;
	le16 size;
	le32 fill1;
	le16 indexsz;
	le16 indexksz;
	le16 flags;
	le16 fill2;
	le32 keyhash;
	le32 keysecurid;

	/* did not find official description for the following */
	le32 hash;
	le32 securid;
	le32 dataoffsl;
	le32 dataoffsh;
	le32 datasize;
	le32 fill3;
	} ;

#ifndef INVALID_FILE_ATTRIBUTES     /* not defined in old windows.h */
#define INVALID_FILE_ATTRIBUTES (-1)
#endif

enum { MAPUSERS, MAPGROUPS, MAPCOUNT } ;

struct SECURITY_CONTEXT {
	struct MAPPING *mapping[MAPCOUNT];
} ;

typedef enum { MAPNONE, MAPEXTERN, MAPLOCAL, MAPDUMMY } MAPTYPE;



struct passwd {
	uid_t pw_uid;
} ;

struct group {
	gid_t gr_gid;
} ;

typedef int (*FILEREADER)(void *fileid, char *buf, size_t size, off_t pos);

/*
 *		Data defined in secaudit.c
 */

extern MAPTYPE mappingtype;

/*
 *		Functions defined in acls.c
 */

BOOL ntfs_valid_descr(const char *securattr, unsigned int attrsz);
BOOL ntfs_valid_posix(const struct POSIX_SECURITY *pxdesc);
BOOL ntfs_valid_pattern(const SID *sid);
BOOL ntfs_same_sid(const SID *first, const SID *second);


int ntfs_sid_size(const SID * sid);
unsigned int ntfs_attr_size(const char *attr);

const SID *ntfs_find_usid(const struct MAPPING *usermapping,
			uid_t uid, SID *pdefsid);
const SID *ntfs_find_gsid(const struct MAPPING *groupmapping,
			gid_t gid, SID *pdefsid);
uid_t ntfs_find_user(const struct MAPPING *usermapping, const SID *usid);
gid_t ntfs_find_group(const struct MAPPING *groupmapping, const SID * gsid);
const SID *ntfs_acl_owner(const char *secattr);

void ntfs_sort_posix(struct POSIX_SECURITY *pxdesc);
int ntfs_merge_mode_posix(struct POSIX_SECURITY *pxdesc, mode_t mode);


struct POSIX_SECURITY *ntfs_build_permissions_posix(
			struct MAPPING* const mapping[],
			const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir);
int ntfs_build_permissions(const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir);
struct MAPLIST *ntfs_read_mapping(FILEREADER reader, void *fileid);
struct MAPPING *ntfs_do_user_mapping(struct MAPLIST *firstitem);
struct MAPPING *ntfs_do_group_mapping(struct MAPLIST *firstitem);
void ntfs_free_mapping(struct MAPPING *mapping[]);

struct POSIX_SECURITY *ntfs_merge_descr_posix(const struct POSIX_SECURITY *first,
			const struct POSIX_SECURITY *second);
char *ntfs_build_descr_posix(struct MAPPING* const mapping[],
			struct POSIX_SECURITY *pxdesc,
			int isdir, const SID *usid, const SID *gsid);
char *ntfs_build_descr(mode_t mode,
			int isdir, const SID * usid, const SID * gsid);

/*
 *		Functions defined in secaudit.c
 */

void *chkmalloc(size_t, const char*, int);
void *chkcalloc(size_t, size_t, const char *, int);
void chkfree(void*, const char*, int);
BOOL chkisalloc(void*, const char*, int);
void dumpalloc(const char*);

#define malloc(sz) chkmalloc(sz, __FILE__, __LINE__)
#define calloc(cnt,sz) chkcalloc(cnt, sz, __FILE__, __LINE__)
#define free(ptr) chkfree(ptr, __FILE__, __LINE__)
#define isalloc(ptr) chkisalloc(ptr, __FILE__, __LINE__)
#define ntfs_malloc(sz) chkmalloc(sz, __FILE__, __LINE__)

struct passwd *getpwnam(const char *user);
struct group *getgrnam(const char *group);

const SID *relay_find_usid(const struct MAPPING *usermapping,
			uid_t uid, SID *pdefsid);
const SID *relay_find_gsid(const struct MAPPING *groupmapping,
			gid_t gid, SID *pdefsid);
uid_t relay_find_user(const struct MAPPING *usermapping, const SID *usid);
gid_t relay_find_group(const struct MAPPING *groupmapping, const SID * gsid);

