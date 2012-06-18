/*
 *		 Display and audit security attributes in an NTFS volume
 *
 * Copyright (c) 2007-2010 Jean-Pierre Andre
 * 
 *	Options :
 *		-a auditing security data
 *		-b backing up NTFS ACLs
 *		-e set extra backed-up parameters (in conjunction with -s)
 *		-h displaying hexadecimal security descriptors within a file
 *		-r recursing in a directory
 *		-s setting backed-up NTFS ACLs
 *		-v verbose (very verbose if set twice)
 *	   also, if compile-time option is set
 *		-t run internal tests (with no access to storage)
 *
 *	On Linux (being root, with volume not mounted) :
 *		secaudit -h [file]
 *			display the security descriptors found in file
 *		secaudit -a[rv] volume
 *			audit the volume
 *		secaudit [-v] volume file
 *			display the security parameters of file
 *		secaudit -r[v] volume directory
 *			display the security parameters of files in directory
 *		secaudit -b[v] volume [directory]
 *			backup the security parameters of files in directory
 *		secaudit -s[ve] volume [backupfile]
 *			set the security parameters as indicated in backup
 *			with -e set extra parameters (Windows attrib)
 *		secaudit volume perms file
 *			set the security parameters of file to perms (mode or acl)
 *		secaudit -r[v] volume perms directory
 *			set the security parameters of files in directory to perms
 *          special case, does not require being root :
 *		secaudit [-v] mounted-file
 *			display the security parameters of mounted file
 *
 *
 *	On Windows (the volume being part of file name)
 *		secaudit -h [file]
 *			display the security descriptors found in file
 *		secaudit [-v] file
 *			display the security parameters of file
 *		secaudit -r[v] directory
 *			display the security parameters of files in directory
 *		secaudit -b[v] directory
 *			backup the security parameters of files in directory
 *		secaudit -s[v] [backupfile]
 *			set the security parameters as indicated in backup
 *			with -e set extra parameters (Windows attrib)
 *		secaudit perms file
 *			set the security parameters of file to perms (mode or acl)
 *		secaudit -r[v] perms directory
 *			set the security parameters of files in directory to perms
 */

/*	History
 *
 *  Nov 2007
 *     - first version, by concatenating miscellaneous utilities
 *
 *  Jan 2008, version 1.0.1
 *     - fixed mode displaying
 *     - added a global severe errors count
 *
 *  Feb 2008, version 1.0.2
 *     - implemented conversions for big-endian machines
 *
 *  Mar 2008, version 1.0.3
 *     - avoided consistency checks on $Secure when there is no such file
 *
 *  Mar 2008, version 1.0.4
 *     - changed ordering of ACE's
 *     - changed representation for special flags
 *     - defaulted to stdin for option -h
 *     - added self tests (compile time option)
 *     - fixed errors specific to big-endian computers
 *
 *  Apr 2008, version 1.1.0
 *     - developped Posix ACLs to NTFS ACLs conversions
 *     - developped NTFS ACLs backup and restore
 *
 *  Apr 2008, version 1.1.1
 *     - fixed an error specific to big-endian computers
 *     - checked hash value and fixed error report in restore
 *     - improved display in showhex() and restore()
 *
 *  Apr 2008, version 1.1.2
 *     - improved and fixed Posix ACLs to NTFS ACLs conversions
 *
 *  Apr 2008, version 1.1.3
 *     - reenabled recursion for setting a new mode or ACL
 *     - processed Unicode file names and displayed them as UTF-8
 *     - allocated dynamically memory for file names when recursing
 *
 *  May 2008, version 1.1.4
 *     - all Unicode/UTF-8 strings checked and processed
 *
 *  Jul 2008, version 1.1.5
 *     - made Windows owner consistent with Linux owner when changing mode
 *     - allowed owner change on Windows when it does not match Linux owner
 *     - skipped currently unused code
 *
 *  Aug 2008, version 1.2.0
 *     - processed \.NTFS-3G\UserMapping on Windows
 *     - made use of user mappings through the security API or direct access
 *     - fixed a bug in restore
 *     - fixed UTF-8 conversions
 *
 *  Sep 2008, version 1.3.0
 *     - split the code to have part of it shared with ntfs-3g library
 *     - fixed testing for end of SDS block
 *     - added samples checking in selftest for easier debugging
 *
 *  Oct 2008, version 1.3.1
 *     - fixed outputting long long data when testing on a Palm organizer
 *
 *  Dec 2008, version 1.3.2
 *     - fixed restoring ACLs
 *     - added optional logging of ACL hashes to facilitate restore checks
 *     - fixed collecting SACLs
 *     - fixed setting special control flags
 *     - fixed clearing existing SACLs (Linux only) and DACLs
 *     - changed the sequencing of items when quering a security descriptor
 *     - avoided recursing on junctions and symlinks on Windows
 *
 *  Jan 2009, version 1.3.3
 *     - save/restore Windows attributes (code from Faisal)
 *
 *  Mar 2009, version 1.3.4
 *     - enabled displaying attributes of a mounted file over Linux
 *
 *  Apr 2009, version 1.3.5
 *     - fixed initialisation of stand-alone user mapping
 *     - fixed POSIXACL redefinition when included in the ntfs-3g package
 *     - fixed displaying of options
 *     - fixed a dependency on the shared library version used
 *
 *  May 2009, version 1.3.6
 *     - added new samples for self testing
 *
 *  Jun 2009, version 1.3.7
 *     - fixed displaying owner and group of a mounted file over Linux
 *
 *  Jul 2009, version 1.3.8
 *     - fixed again displaying owner and group of a mounted file over Linux
 *     - cleaned some code to avoid warnings
 *
 *  Nov 2009, version 1.3.9
 *     - allowed security descriptors up to 64K
 *
 *  Nov 2009, version 1.3.10
 *     - applied patches for MacOSX from Erik Larsson
 *
 *  Nov 2009, version 1.3.11
 *     - replace <attr/xattr.h> by <sys/xattr.h> (provided by glibc)
 *
 *  Dec 2009, version 1.3.12
 *     - worked around "const" possibly redefined in config.h
 *
 *  Dec 2009, version 1.3.13
 *     - fixed the return code of dorestore()
 *
 *  Dec 2009, version 1.3.14
 *     - adapted to opensolaris
 *
 *  Jan 2010, version 1.3.15
 *     - more adaptations to opensolaris
 *     - removed the fix for return code of dorestore()
 *
 *  Jan 2010, version 1.3.16
 *     - repeated the fix for return code of dorestore()
 *
 *  Mar 2010, version 1.3.17
 *     - adapted to new default user mapping
 *     - fixed #ifdef'd code for selftest
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

#define AUDT_VERSION "1.3.17"

#define GET_FILE_SECURITY "ntfs_get_file_security"
#define SET_FILE_SECURITY "ntfs_set_file_security"
#define GET_FILE_ATTRIBUTES "ntfs_get_file_attributes"
#define SET_FILE_ATTRIBUTES "ntfs_set_file_attributes"
#define READ_DIRECTORY "ntfs_read_directory"
#define READ_SDS "ntfs_read_sds"
#define READ_SII "ntfs_read_sii"
#define READ_SDH "ntfs_read_sdh"
#define GET_USER "ntfs_get_user"
#define GET_GROUP "ntfs_get_group"
#define GET_USID "ntfs_get_usid"
#define GET_GSID "ntfs_get_gsid"
#define INIT_FILE_SECURITY "ntfs_initialize_file_security"
#define LEAVE_FILE_SECURITY "ntfs_leave_file_security"

/*
 *			External declarations
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

				/*
				 * integration into secaudit, check whether Win32,
				 * may have to be adapted to compiler or something else
				 */

#ifndef WIN32
#if defined(__WIN32) | defined(__WIN32__) | defined(WNSC)
#define WIN32 1
#endif
#endif

				/*
				 * integration into secaudit/Win32
				 */
#ifdef WIN32
#include <windows.h>
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
#else
				/*
				 * integration into secaudit/STSC
				 */
#ifdef STSC
#include <stat.h>
#undef __BYTE_ORDER
#define __BYTE_ORDER __BIG_ENDIAN
#else
				/*
				 * integration into secaudit/Linux
				 */

#include <sys/stat.h>
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif
#include <unistd.h>
#include <dlfcn.h>
#endif /* STSC */
#endif /* WIN32 */
#include "secaudit.h"

#ifndef WIN32

#ifndef STSC

#if !defined(HAVE_CONFIG_H) && POSIXACLS
      /* require <sys/xattr.h> if not integrated into ntfs-3g package */
#define HAVE_SETXATTR 1
#endif

#ifdef HAVE_CONFIG_H
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS  /* work around "_FILE_OFFSET_BITS" possibly already defined */
#endif
      /* <sys/xattr.h> according to config.h if integrated into ntfs-3g package */
#include "config.h"
#ifdef const /* work around "const" possibly redefined in config.h */
#undef const
#endif
#ifndef POSIXACLS
#define POSIXACLS 0
#endif
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#else
#warning "The extended attribute package is not available"
#endif /* HAVE_SETXATTR */

#endif /* STSC */

#define MS_NONE 0    /* no flag for mounting the device */
#define MS_RDONLY 1  /* flag for mounting the device read-only */

struct CALLBACK;

typedef int (*dircallback)(struct CALLBACK *context, char *ntfsname,
	int length, int type, long long pos, u64 mft_ref,
	unsigned int dt_type);

#if USESTUBS | defined(STSC)

int ntfs_get_file_security(void *scapi,
		const char *path, DWORD selection,
		char *buf, DWORD buflen, LPDWORD psize);
BOOL ntfs_set_file_security(void *scapi,
		const char *path, DWORD selection, const char *attr);
int ntfs_get_file_attributes(void *scapi,
		const char *path);
BOOL ntfs_set_file_attributes(void *scapi,
		const char *path, DWORD attrib);
BOOL ntfs_read_directory(void *scapi,
		const char *path, dircallback callback, void *context);
int ntfs_read_sds(void *scapi,
		char *buf, DWORD buflen, DWORD offset);
void *ntfs_read_sii(void *scapi, void *entry);
void *ntfs_read_sdh(void *scapi, void *entry);

int ntfs_get_usid(void *scapi, uid_t uid, char *buf);
int ntfs_get_gsid(void *scapi, gid_t gid, char *buf);
int ntfs_get_user(void *scapi, const char *usid);
int ntfs_get_group(void *scapi, const char *gsid);

void *ntfs_initialize_file_security(const char *device, int flags);
BOOL ntfs_leave_file_security(void *scapi);

#else

typedef int (*type_get_file_security)(void *scapi,
		const char *path, DWORD selection,
		char *buf, DWORD buflen, LPDWORD psize);
typedef BOOL (*type_set_file_security)(void *scapi,
		const char *path, DWORD selection, const char *attr);
typedef int (*type_get_file_attributes)(void *scapi,
		const char *path);
typedef BOOL (*type_set_file_attributes)(void *scapi,
		const char *path, DWORD attrib);
typedef BOOL (*type_read_directory)(void *scapi,
		const char *path, dircallback callback, void *context);
typedef int (*type_read_sds)(void *scapi,
		char *buf, DWORD buflen, DWORD offset);
typedef void *(*type_read_sii)(void *scapi, void *entry);
typedef void *(*type_read_sdh)(void *scapi, void *entry);

typedef int (*type_get_usid)(void *scapi, uid_t uid, char *buf);
typedef int (*type_get_gsid)(void *scapi, gid_t gid, char *buf);
typedef int (*type_get_user)(void *scapi, const char *usid);
typedef int (*type_get_group)(void *scapi, const char *gsid);

typedef void *(*type_initialize_file_security)(const char *device, int flags);
typedef BOOL (*type_leave_file_security)(void *scapi);

type_get_file_security ntfs_get_file_security;
type_set_file_security ntfs_set_file_security;
type_get_file_attributes ntfs_get_file_attributes;
type_set_file_attributes ntfs_set_file_attributes;
type_read_directory ntfs_read_directory;
type_read_sds ntfs_read_sds;
type_read_sii ntfs_read_sii;
type_read_sdh ntfs_read_sdh;

type_get_usid ntfs_get_usid;
type_get_gsid ntfs_get_gsid;
type_get_user ntfs_get_user;
type_get_group ntfs_get_group;

type_initialize_file_security ntfs_initialize_file_security;
type_leave_file_security ntfs_leave_file_security;


#endif /* USESTUBS | defined(STSC) */
#endif /* WIN32 */

/*
 *		Prototypes for local functions
 */

BOOL open_security_api(void);
BOOL close_security_api(void);
#ifndef WIN32
BOOL open_volume(const char*, int flags);
BOOL close_volume(const char*);
#endif
unsigned int get2l(const char*, int);
unsigned long get4l(const char*, int);
u64 get6l(const char*, int);
u64 get6h(const char*, int);
u64 get8l(const char*, int);
void set2l(char*, unsigned int);
void set4l(char*, unsigned long);
void hexdump(const char*, int, int);
u32 hash(const le32*, int);
unsigned int utf8size(const char*, int);
unsigned int makeutf8(char*, const char*, int);
unsigned int utf16size(const char*);
unsigned int makeutf16(char*, const char*);
unsigned int utf16len(const char*);
void printname(FILE*, const char*);
void printerror(FILE*);
BOOL guess_dir(const char*);
void showsid(const char*, int, int);
void showusid(const char*, int);
void showgsid(const char*, int);
void showheader(const char*, int);
void showace(const char*, int, int, int);
void showacl(const char*, int, int, int);
void showdacl(const char*, int, int);
void showsacl(const char*, int, int);
void showall(const char*, int);
void showposix(const struct POSIX_SECURITY*);
int linux_permissions(const char*, BOOL);
uid_t linux_owner(const char*);
gid_t linux_group(const char*);
int basicread(void*, char*, size_t, off_t);
int dummyread(void*, char*, size_t, off_t);
int local_build_mapping(struct MAPPING *[], const char*);
void newblock(s32);
void freeblocks(void);
u32 getmsbhex(const char*);
u32 getlsbhex(const char*);
BOOL ishexdump(const char*, int, int);
void showhex(FILE*);
void showfull(const char*, BOOL);
BOOL applyattr(const char*, const char*, BOOL, int, s32);
BOOL restore(FILE*);
BOOL dorestore(const char*, FILE*);
u32 merge_rights(const struct POSIX_SECURITY*, BOOL);
void tryposix(struct POSIX_SECURITY*);
void check_samples(void);
void basictest(int, BOOL, const SID*, const SID*);
void posixtest(int, BOOL, const SID*, const SID*);
void selftests(void);
unsigned int getfull(char*, const char*);
BOOL updatefull(const char *name, DWORD flags, char *attr);
BOOL setfull(const char*, int, BOOL);
BOOL singleshow(const char*);
void showmounted(const char*);
BOOL recurseshow(const char*);
BOOL singleset(const char*, int);
BOOL recurseset(const char*, int);
#ifdef WIN32
BOOL backup(const char*);
BOOL listfiles(const char*);
#if POSIXACLS
BOOL iterate(RECURSE, const char*, const struct POSIX_SECURITY*);
#else
BOOL iterate(RECURSE, const char*, mode_t);
#endif
#else
BOOL backup(const char*, const char*);
BOOL listfiles(const char*, const char*);
#endif
#if POSIXACLS
BOOL setfull_posix(const char *, const struct POSIX_SECURITY*, BOOL);
struct POSIX_SECURITY *linux_permissions_posix(const char*, BOOL);
BOOL recurseset_posix(const char*, const struct POSIX_SECURITY*);
BOOL singleset_posix(const char*, const struct POSIX_SECURITY*);
struct POSIX_SECURITY *encode_posix_acl(const char*);
#endif
static void stdfree(void*);

BOOL valid_sds(const char*, unsigned int, unsigned int,
		unsigned int, u32, BOOL);
BOOL valid_sii(const char*, u32);
BOOL valid_sdh(const char*, u32, u32);
int consist_sds(const char*, unsigned int, unsigned int, BOOL);
int consist_sii(const char*);
int consist_sdh(const char*);
int audit_sds(BOOL);
int audit_sii(void);
int audit_sdh(void);
void audit_summary(void);
BOOL audit(const char*);
int getoptions(int, char*[]);

#ifndef WIN32

/*
 *		Structures for collecting directory contents (Linux only)
 */

struct LINK {
	struct LINK *next;
	char name[1];
} ;

struct CALLBACK {
	struct LINK *head;
	const char *dir;
} ;

int callback(struct CALLBACK *context, char *ntfsname,
	int length, int type, long long pos, u64 mft_ref,
	unsigned int dt_type);
#endif

/*
 *		  Global constants
 */

#define BANNER "secaudit " AUDT_VERSION " : NTFS security data auditing"

#if SELFTESTS & !USESTUBS

/*
 *		Dummy mapping file (self tests only)
 */

#define DUMMYAUTH "S-1-5-21-3141592653-589793238-462843383-"
char dummymapping[] =	"500::" DUMMYAUTH "1000\n"
			"501::" DUMMYAUTH "1001\n"
			"502::" DUMMYAUTH "1002\n"
			"503::" DUMMYAUTH "1003\n"
			"516::" DUMMYAUTH "1016\n"
			":500:" DUMMYAUTH "513\r\n"
			":511:S-1-5-21-1607551490-981732888-1819828000-513\n"
			":516:" DUMMYAUTH "1012\r\n"
			"::"	DUMMYAUTH "10000\n";

/*
 *		SID for world  (S-1-1-0)
 */

static const char worldsidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 1,	/* base */
		0, 0, 0, 0	/* 1st level */
} ;
static const SID *worldsid = (const SID*)worldsidbytes;

/*
 *		SID for administrator
 */

static const char adminsidbytes[] = {
		1,		/* revision */
		2,		/* auth count */
		0, 0, 0, 0, 0, 5,	/* base */
		32, 0, 0, 0,	/* 1st level */
		32, 2, 0, 0	/* 2nd level */
};

static const SID *adminsid = (const SID*)adminsidbytes;

/*	        
 *		SID for system
 */
	        
static const char systemsidbytes[] = {
		1,		/* revision */ 
		1,		/* auth count */
		0, 0, 0, 0, 0, 5,	/* base */
		18, 0, 0, 0	/* 1st level */
	};
  
static const SID *systemsid = (const SID*)systemsidbytes;

#endif

/*
 *		  Global variables
 */

BOOL opt_a;	/* audit security data */
BOOL opt_b;	/* backup NTFS ACLs */
BOOL opt_e;	/* restore extra (currently windows attribs) */
BOOL opt_h;	/* display an hexadecimal descriptor in a file */
BOOL opt_r;	/* recursively apply to subdirectories */
BOOL opt_s;	/* restore NTFS ACLs */
#if SELFTESTS & !USESTUBS
BOOL opt_t;	/* run self-tests */
#endif
int opt_v;  /* verbose or very verbose*/
struct SECURITY_DATA *securdata[(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ)];
#if FORCEMASK
BOOL opt_m;	/* force a mask - dangerous */
u32 forcemsk /* mask to force */
#endif
unsigned int errors; /* number of severe errors */
unsigned int warnings; /* number of non-severe errors */

struct CHKALLOC *firstalloc;
struct SECURITY_CONTEXT context;
MAPTYPE mappingtype;

#ifdef STSC
#define static
#endif

#ifndef WIN32

void *ntfs_handle;
void *ntfs_context = (void*)NULL;

/*
 *		Open and close the security API (platform dependent)
 */

BOOL open_security_api(void)
{
#if USESTUBS | defined(STSC)
	return (TRUE);
#else
	char *error;
	BOOL err;
	const char *libfile;

	err = TRUE;
	libfile = getenv(ENVNTFS3G);
	if (!libfile)
		libfile = (sizeof(char*) == 8 ? LIBFILE64 : LIBFILE);
	ntfs_handle = dlopen(libfile,RTLD_LAZY);
	if (ntfs_handle) {
		ntfs_initialize_file_security = (type_initialize_file_security)
				dlsym(ntfs_handle,INIT_FILE_SECURITY);
		error = dlerror();
		if (error)
			fprintf(stderr," %s\n",error);
		else {
			ntfs_leave_file_security = (type_leave_file_security)
					dlsym(ntfs_handle,LEAVE_FILE_SECURITY);
			ntfs_get_file_security = (type_get_file_security)
					dlsym(ntfs_handle,GET_FILE_SECURITY);
			ntfs_set_file_security = (type_set_file_security)
					dlsym(ntfs_handle,SET_FILE_SECURITY);
			ntfs_get_file_attributes = (type_get_file_attributes)
					dlsym(ntfs_handle,GET_FILE_ATTRIBUTES);
			ntfs_set_file_attributes = (type_set_file_attributes)
					dlsym(ntfs_handle,SET_FILE_ATTRIBUTES);
			ntfs_read_directory = (type_read_directory)
					dlsym(ntfs_handle,READ_DIRECTORY);
			ntfs_read_sds = (type_read_sds)
					dlsym(ntfs_handle,READ_SDS);
			ntfs_read_sii = (type_read_sii)
					dlsym(ntfs_handle,READ_SII);
			ntfs_read_sdh = (type_read_sdh)
					dlsym(ntfs_handle,READ_SDH);
			ntfs_get_user = (type_get_user)
					dlsym(ntfs_handle,GET_USER);
			ntfs_get_group = (type_get_group)
					dlsym(ntfs_handle,GET_GROUP);
			ntfs_get_usid = (type_get_usid)
					dlsym(ntfs_handle,GET_USID);
			ntfs_get_gsid = (type_get_gsid)
					dlsym(ntfs_handle,GET_GSID);
			err = !ntfs_initialize_file_security
				|| !ntfs_leave_file_security
				|| !ntfs_get_file_security
				|| !ntfs_set_file_security
				|| !ntfs_get_file_attributes
				|| !ntfs_set_file_attributes
				|| !ntfs_read_directory
				|| !ntfs_read_sds
				|| !ntfs_read_sii
				|| !ntfs_read_sdh
				|| !ntfs_get_user
				|| !ntfs_get_group
				|| !ntfs_get_usid
				|| !ntfs_get_gsid;

			if (error)
				fprintf(stderr,"ntfs-3g API not available\n");
		}
	} else {
		fprintf(stderr,"Could not open ntfs-3g library\n");
		fprintf(stderr,"\nPlease set environment variable \"" ENVNTFS3G "\"\n");
		fprintf(stderr,"to appropriate path and retry\n");
	}
	return (!err);
#endif /* USESTUBS | defined(STSC) */
}

BOOL close_security_api(void)
{
#if USESTUBS | defined(STSC)
	return (0);
#else
	return (!dlclose(ntfs_handle));
#endif /* USESTUBS | defined(STSC) */
}

/*
 *		Open and close a volume (platform dependent)
 *	Assumes a single volume is opened
 */

BOOL open_volume(const char *volume, int flags)
{
	BOOL ok;

	ok = FALSE;
	if (!ntfs_context) {
		ntfs_context = (*ntfs_initialize_file_security)(volume,flags);
		if (ntfs_context) {
			if (*(u32*)ntfs_context != MAGIC_API) {
				fprintf(stderr,"Versions of ntfs-3g and secaudit"
						" are not compatible\n");
			} else {
				fprintf(stderr,"\"%s\" opened\n",volume);
				mappingtype = MAPEXTERN;
				ok = TRUE;
			}
		} else {
			fprintf(stderr,"Could not open \"%s\"\n",volume);
			fprintf(stderr,"Make sure \"%s\" is not mounted\n",volume);
		}
	} else
		fprintf(stderr,"A volume is already open\n");
	return (ok);
}

BOOL close_volume(const char *volume)
{
	BOOL r;

	r = (*ntfs_leave_file_security)(ntfs_context);
	if (r)
		fprintf(stderr,"\"%s\" closed\n",volume);
	else
		fprintf(stderr,"Could not close \"%s\"\n",volume);
	ntfs_context = (void*)NULL;
	return (r);
}

#endif /* WIN32 */

/*
 *		Extract small or big endian data from an array of bytes
 */

unsigned int get2l(const char *attr, int p)
{
	int i;
	unsigned int v;

	v = 0;
	for (i=0; i<2; i++)
		v += (attr[p+i] & 255) << (8*i);
	return (v);
}

unsigned long get4l(const char *attr, int p)
{
	int i;
	unsigned long v;

	v = 0;
	for (i=0; i<4; i++)
		v += ((long)(attr[p+i] & 255)) << (8*i);
	return (v);
}

u64 get6l(const char *attr, int p)
{
	int i;
	u64 v;

	v = 0;
	for (i=0; i<6; i++)
		v += ((long long)(attr[p+i] & 255)) << (8*i);
	return (v);
}

u64 get6h(const char *attr, int p)
{
	int i;
	u64 v;

	v = 0;
	for (i=0; i<6; i++)
		v = (v << 8) + (attr[p+i] & 255);
	return (v);
}

u64 get8l(const char *attr, int p)
{
	int i;
	u64 v;

	v = 0;
	for (i=0; i<8; i++)
		v += ((long long)(attr[p+i] & 255)) << (8*i);
	return (v);
}

/*
 *		Set small or big endian data into an array of bytes
 */

void set2l(char *p, unsigned int v)
{
	int i;

	for (i=0; i<2; i++)
		p[i] = ((v >> 8*i) & 255);
}

void set4l(char *p, unsigned long v)
{
	int i;

	for (i=0; i<4; i++)
		p[i] = ((v >> 8*i) & 255);
}


/*
 *		hexadecimal dump of an array of bytes
 */

void hexdump(const char *attr, int size, int level)
{
	int i,j;

	for (i=0; i<size; i+=16) {
		if (level)
			printf("%*c",level,' ');
		printf("%06x ",i);
		for (j=i; (j<(i+16)) && (j<size); j++)
			printf((j & 3 ? "%02x" : " %02x"),attr[j] & 255);
		printf("\n");
	}
}

u32 hash(const le32 *buf, int size /* bytes */)
{
	u32 h;
	int i;

	h = 0;
	for (i=0; 4*i<size; i++)
		h = le32_to_cpu(buf[i]) + (h << 3) + ((h >> 29) & 7);
	return (h);
}

/*
 *		Evaluate the size of UTS-8 conversion of a UTF-16LE text
 *	trailing '\0' not accounted for
 *	Returns 0 for invalid input
 */

unsigned int utf8size(const char *utf16, int length)
{
	int i;
	unsigned int size;
	enum { BASE, SURR, ERR } state;

	size = 0;
	state = BASE;
	for (i=0; i<2*length; i+=2) {
		switch (state) {
		case BASE :
			if (utf16[i+1] & 0xf8) {
				if ((utf16[i+1] & 0xf8) == 0xd8)
					state = (utf16[i+1] & 4 ? ERR : SURR);
				else
#if NOREVBOM
					if (((utf16[i+1] & 0xff) == 0xff)
					  && ((utf16[i] & 0xfe) == 0xfe))
						state = ERR;
					else
						size += 3;
#else
					size += 3;
#endif
			} else
				if ((utf16[i] & 0x80) || utf16[i+1])
					size += 2;
				else
					size++;
			break;
		case SURR :
			if ((utf16[i+1] & 0xfc) == 0xdc) {
				state = BASE;
				size += 4;
			} else
				state = ERR;
			break;
		case ERR :
			break;
		}
	}
	if (state != BASE)
		size = 0;
	return (size);
}

/*
 *		Convert a UTF-16LE text to UTF-8
 *	Note : wcstombs() not used because on Linux it fails for characters
 *	not present in current locale
 *	Returns size or zero for invalid input
 */

unsigned int makeutf8(char *utf8, const char *utf16, int length)
{
	int i;
	unsigned int size;
	unsigned int rem;
	enum { BASE, SURR, ERR } state;

	size = 0;
	rem = 0;
	state = BASE;
	for (i=0; i<2*length; i+=2) {
		switch (state) {
		case BASE :
			if (utf16[i+1] & 0xf8) {
				if ((utf16[i+1] & 0xf8) == 0xd8) {
					if (utf16[i+1] & 4)
						state = ERR;
					else {
						utf8[size++] = 0xf0 + (utf16[i+1] & 7)
                                                                    + ((utf16[i] & 0xc0) == 0xc0);
						utf8[size++] = 0x80 + (((utf16[i] + 64) >> 2) & 63);
						rem = utf16[i] & 3;
						state = SURR;
					}
				} else {
#if NOREVBOM
					if (((utf16[i+1] & 0xff) == 0xff)
					  && ((utf16[i] & 0xfe) == 0xfe))
						state = ERR;
					else {
						utf8[size++] = 0xe0 + ((utf16[i+1] >> 4) & 15);
						utf8[size++] = 0x80
							+ ((utf16[i+1] & 15) << 2)
							+ ((utf16[i] >> 6) & 3);
						utf8[size++] = 0x80 + (utf16[i] & 63);
					}
#else
					utf8[size++] = 0xe0 + ((utf16[i+1] >> 4) & 15);
					utf8[size++] = 0x80
						+ ((utf16[i+1] & 15) << 2)
						+ ((utf16[i] >> 6) & 3);
					utf8[size++] = 0x80 + (utf16[i] & 63);
#endif
				}
			} else
				if ((utf16[i] & 0x80) || utf16[i+1]) {
					utf8[size++] = 0xc0
						+ ((utf16[i+1] & 15) << 2)
						+ ((utf16[i] >> 6) & 3);
					utf8[size++] = 0x80 + (utf16[i] & 63);
				} else
					utf8[size++] = utf16[i];
			break;
		case SURR :
			if ((utf16[i+1] & 0xfc) == 0xdc) {
				utf8[size++] = 0x80 + (rem << 4)
						 + ((utf16[i+1] & 3) << 2)
						 + ((utf16[i] >> 6) & 3);
				utf8[size++] = 0x80 + (utf16[i] & 63);
				state = BASE;
			} else
				state = ERR;
			break;
		case ERR :
			break;
		}
	}
	utf8[size] = 0;
	if (state != BASE)
		state = ERR;
	return (state == ERR ? 0 : size);
}

#ifdef WIN32

/*
 *		Evaluate the size of UTF-16LE conversion of a UTF-8 text
 *	(basic conversions only)
 *	trailing '\0' not accounted for
 */

unsigned int utf16size(const char *utf8)
{
	unsigned int size;
	const char *p;
	int c;
	unsigned int code;
	enum { BASE, TWO, THREE, THREE2, THREE3, FOUR, ERR } state;

	p = utf8;
	size = 0;
	state = BASE;
	while (*p) {
		c = *p++ & 255;
		switch (state) {
		case BASE :
			if (!(c & 0x80))
				size++;
			else
				if (c < 0xc2)
					state = ERR;
				else
					if (c < 0xe0)
						state = TWO;
					else
						if (c < 0xf0) {
							if (c == 0xe0)
								state = THREE2;
							else
								if (c == 0xed)
									state = THREE3;
								else
									state = THREE;
						} else
							if (c < 0xf8) {
								state = FOUR;
								code = c & 7;
							} else
								state = ERR;
			break;
		case TWO :
			if ((c & 0xc0) != 0x80)
				state = ERR;
			else {
				size++;
				state = BASE;
			}
			break;
		case THREE :
			if ((c & 0xc0) != 0x80)
				state = ERR;
			else
				state = TWO;
			break;
		case THREE2 :
			if ((c & 0xe0) != 0xa0)
				state = ERR;
			else
				state = TWO;
			break;
		case THREE3 :
			if ((c & 0xe0) != 0x80)
				state = ERR;
			else
				state = TWO;
			break;
		case FOUR :
			if ((((code << 6) + (c & 63)) > 0x10f)
			  || (((code << 6) + (c & 63)) < 0x10))
				state = ERR;
			else {
				size++;
				state = THREE;
			}
			break;
		case ERR :
			break;
		}
	}
	if (state != BASE) size = 0;
	return (size);
}

/*
 *		Convert a UTF8 text to UTF-16LE
 *	(basic conversions only)
 *	Note : mbstowcs() not used because on Linux it fails for characters
 *	not present in current locale
 */

unsigned int makeutf16(char *target, const char *utf8)
{
	unsigned int size;
	unsigned int code;
	const char *p;
	int c;
	enum { BASE, TWO, THREE, THREE2, THREE3, FOUR, FOUR2, FOUR3, ERR } state;

	p = utf8;
	size = 0;
	c = 0;
	state = BASE;
	while (*p) {
		c = *p++ & 255;
		switch (state) {
		case BASE :
			if (!(c & 0x80)) {
				target[2*size] = c;
				target[2*size + 1] = 0;
				size++;
			} else {
				if (c < 0xc2)
					state = ERR;
				else
					if (c < 0xe0) {
						code = c & 31;
						state = TWO;
					} else
						if (c < 0xf0) {
							code = c & 15;
							if (c == 0xe0)
								state = THREE2;
							else
								if (c == 0xed)
									state = THREE3;
								else
									state = THREE;
						} else
							if (c < 0xf8) {
								code = c & 7;
								state = FOUR;
							} else
								state = ERR;
			}
			break;
		case TWO :
#if NOREVBOM
			if (((c & 0xc0) != 0x80)
			  || ((code == 0x3ff) && (c >= 0xbe)))
#else
			if ((c & 0xc0) != 0x80)
#endif
				state = ERR;
			else {
				target[2*size] = ((code & 3) << 6) + (c & 63);
				target[2*size + 1] = ((code >> 2) & 255);
				size++;
				state = BASE;
			}
			break;
		case THREE :
			if ((c & 0xc0) != 0x80)
				state = ERR;
			else {
				code = ((code & 15) << 6) + (c & 63);
				state = TWO;
			}
			break;
		case THREE2 :
			if ((c & 0xe0) != 0xa0)
				state = ERR;
			else {
				code = ((code & 15) << 6) + (c & 63);
				state = TWO;
			}
			break;
		case THREE3 :
			if ((c & 0xe0) != 0x80)
				state = ERR;
			else {
				code = ((code & 15) << 6) + (c & 63);
				state = TWO;
			}
			break;
		case FOUR :
			if ((c & 0xc0) != 0x80)
				state = ERR;
			else {
				code = (code << 6) + (c & 63);
				state = FOUR2;
			}
			break;
		case FOUR2 :
			if ((c & 0xc0) != 0x80)
				state = ERR;
			else {
				code = (code << 6) + (c & 63);
				state = FOUR3;
			}
			break;
		case FOUR3 :
			if ((code > 0x43ff)
			 || (code < 0x400)
			 || ((c & 0xc0) != 0x80))
				state = ERR;
			else {
				target[2*size] = ((code - 0x400) >> 4) & 255;
				target[2*size+1] = 0xd8 + (((code - 0x400) >> 12) & 3);
				target[2*size+2] = ((code & 3) << 6) + (c & 63);
				target[2*size+3] = 0xdc + ((code >> 2) & 3);
				size += 2;
				state = BASE;
			}
			break;
		case ERR :
			break;
		}
	}
	if (state != BASE)
		size = 0;
	target[2*size] = 0;
	target[2*size + 1] = 0;
	return (size);
}

unsigned int utf16len(const char *str)
{
	unsigned int len;

	len = 0;
	while (str[2*len] || str[2*len+1]) len++;
	return (len);
}

#endif

/*
 *		Print a file name
 *	on Windows it prints UTF-16LE names as UTF-8
 */

void printname(FILE *file, const char *name)
{
#ifdef WIN32
	char utf8name[MAXFILENAME];

	makeutf8(utf8name,name,utf16len(name));
	fprintf(file,"%s",utf8name);	
#else
	fprintf(file,"%s",name);
#endif
}

/*
 *		Print the last error code
 */

void printerror(FILE *file)
{
#ifdef WIN32
	int err;
	const char *txt;

	err = GetLastError();
	switch (err) {
	case 5 :
		txt = "Access to security descriptor was denied";
		break;
	case 1307 :
		txt = "This SID may not be assigned as the owner of this object";
		break;
	case 1308 :
		txt = "This SID may not be assigned as the group of this object";
		break;
	case 1314 :
		txt = "You do not have the privilege to change this SID";
		break;
	default :
		txt = (const char*)NULL;
		break;
	}
	if (txt)
		fprintf(file,"Error %d : %s\n",err,txt);
	else
		fprintf(file,"Error %d\n",err);
#else
#ifdef STSC
	if (errno) fprintf(file,"Error code %d\n",errno);
#else
	if (errno) fprintf(file,"Error code %d : %s\n",errno,strerror(errno));
#endif
#endif
}

/*
 *	Guess whether a security attribute is intended for a directory
 *	based on the presence of inheritable ACE
 *	(not 100% reliable)
 */

BOOL guess_dir(const char *attr)
{
	int off;
	int isdir;
	int cnt;
	int i;
	int x;

	isdir = 0;
	off = get4l(attr,16);
	if (off) {
		cnt = get2l(attr,off+4);
		x = 8;
		for (i=0; i<cnt; i++) {
			if (attr[off + x + 1] & 3)
				isdir = 1;
			x += get2l(attr,off + x + 2);
		}
	}
	return (isdir);
}

/*
 *		   Display a SID
 *   See http://msdn2.microsoft.com/en-us/library/aa379649.aspx
 */

void showsid(const char *attr, int off, int level)
{
	int cnt;
	int i;
	BOOL known;
	u64 auth;
	unsigned long first;
	unsigned long second;
	unsigned long last;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	cnt = attr[off+1] & 255;
	auth = get6h(attr,off+2);
	first = get4l(attr,off+8);
	known = FALSE;
	if ((attr[off] == 1) /* revision */
	     && (auth < 100))
		switch (cnt) {
		case 0 : /* no level (error) */
			break;
		case 1 : /* single level */
			switch (auth) {
			case 0 :
				if (first == 0) {
					known = TRUE;
					printf("%*cNull SID\n",-level,marker);
				}
				break;
			case 1 :
				if (first == 0) {
					known = TRUE;
					printf("%*cWorld SID\n",-level,marker);
				}
				break;
			case 3 :
				switch (first) {
				case 0 :
					known = TRUE;
					printf("%*cCreator owner SID\n",-level,marker);
					break;
				case 1 :
					known = TRUE;
					printf("%*cCreator group SID\n",-level,marker);
					break;
				}
				break;
			case 5 :
				switch (first) {
				case 7 :
					known = TRUE;
					printf("%*cAnonymous logon SID\n",-level,marker);
					break;
				case 11 :
					known = TRUE;
					printf("%*cAuthenticated user SID\n",-level,marker);
					break;
				case 13 :
					known = TRUE;
					printf("%*cLocal service SID\n",-level,marker);
					break;
				case 14 :
					known = TRUE;
					printf("%*cNetwork service SID\n",-level,marker);
					break;
				case 18 :
					known = TRUE;
					printf("%*cNT System SID\n",-level,marker);
					break;
				}
				break;
			}
			break;
		case 2 : /* double level */
			second = get4l(attr,off+12);
			switch (auth) {
			case 5 :
				if (first == 32) {
					known = TRUE;
					switch (second) {
					case 544 :
						printf("%*cLocal admins SID\n",-level,marker);
						break;
					case 545 :
						printf("%*cLocal users SID\n",-level,marker);
						break;
					case 546 :
						printf("%*cLocal guests SID\n",-level,marker);
						break;
					default :
						printf("%*cSome domain SID\n",-level,marker);
						break;
					}
				}
				break;
			}
		default : /* three levels or more */
			second = get4l(attr,off+12);
			last = get4l(attr,off+4+4*cnt);
			switch (auth) {
			case 5 :
				if (first == 21) {
					known = TRUE;
					switch (last)
						{
						case 512 :
							printf("%*cLocal admins SID\n",-level,marker);
							break;
						case 513 :
							printf("%*cLocal users SID\n",-level,marker);
							break;
						case 514 :
							printf("%*cLocal guests SID\n",-level,marker);
							break;
						default :
							printf("%*cLocal user-%lu SID\n",-level,marker,last);
							break;
					}
				}
				break;
			}
		}
	if (!known)
		printf("%*cUnknown SID\n",-level,marker);
	printf("%*chex S-%d-",-level,marker,attr[off] & 255);
	printf("%llx",auth);
	for (i=0; i<cnt; i++)
		printf("-%lx",get4l(attr,off+8+4*i));
	printf("\n");
	printf("%*cdec S-%d-",-level,marker,attr[off] & 255);
	printf("%llu",auth);
	for (i=0; i<cnt; i++)
		printf("-%lu",get4l(attr,off+8+4*i));
	printf("\n");
}

void showusid(const char *attr, int level)
{
	int off;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	if (level)
		printf("%*c",-level,marker);
	printf("User SID\n");
	off = get4l(attr,4);
	showsid(attr,off,level+4);
}

void showgsid(const char *attr, int level)
{
	int off;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	if (level)
		printf("%*c",-level,marker);
	printf("Group SID\n");
	off = get4l(attr,8);
	showsid(attr,off,level+4);
}

void showheader(const char *attr, int level)
{
	int flags;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	if (level)
		printf("%*c",-level,marker);
	printf("Global header\n");
	printf("%*crevision %d\n",-level-4,marker,attr[0]);
	flags = get2l(attr,2);
	printf("%*cflags    0x%x\n",-level-4,marker,flags);
	if (flags & 1)
		printf("%*c    owner is defaulted\n",-level-4,marker);
	if (flags & 2)
		printf("%*c    group is defaulted\n",-level-4,marker);
	if (flags & 4)
		printf("%*c    DACL present\n",-level-4,marker);
	if (flags & 8)
		printf("%*c    DACL is defaulted\n",-level-4,marker);
	if (flags & 0x10)
		printf("%*c    SACL present\n",-level-4,marker);
	if (flags & 0x20)
		printf("%*c    SACL is defaulted\n",-level-4,marker);
	if (flags & 0x100)
		printf("%*c    DACL inheritance is requested\n",-level-4,marker);
	if (flags & 0x200)
		printf("%*c    SACL inheritance is requested\n",-level-4,marker);
	if (flags & 0x400)
		printf("%*c    DACL was inherited automatically\n",-level-4,marker);
	if (flags & 0x800)
		printf("%*c    SACL was inherited automatically\n",-level-4,marker);
	if (flags & 0x1000)
		printf("%*c    DACL cannot be modified by inheritable ACEs\n",-level-4,marker);
	if (flags & 0x2000)
		printf("%*c    SACL cannot be modified by inheritable ACEs\n",-level-4,marker);
	if (flags & 0x8000)
		printf("%*c    self relative descriptor\n",-level-4,marker);
	if (flags & 0x43eb)
		printf("%*c    unknown flags 0x%x present\n",-level-4,marker,
				flags & 0x43eb);
	printf("%*cOff USID 0x%x\n",-level-4,marker,(int)get4l(attr,4));
	printf("%*cOff GSID 0x%x\n",-level-4,marker,(int)get4l(attr,8));
	printf("%*cOff SACL 0x%x\n",-level-4,marker,(int)get4l(attr,12));
	printf("%*cOff DACL 0x%x\n",-level-4,marker,(int)get4l(attr,16));
}

void showace(const char *attr, int off, int isdir, int level)
{
	int flags;
	u32 rights;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	printf("%*ctype     %d\n",-level,marker,attr[off]);
	switch (attr[off]) {
	case 0 :
		printf("%*cAccess allowed\n",-level-4,marker);
		break;
	case 1 :
		printf("%*cAccess denied\n",-level-4,marker);
		break;
	case 2 :
		printf("%*cSystem audit\n",-level-4,marker);
		break;
	default :
		printf("%*cunknown\n",-level-4,marker);
		break;
	}
	flags = attr[off+1] & 255;
	printf("%*cflags    0x%x\n",-level,marker,flags);
	if (flags & 1)
		printf("%*cObject inherits ACE\n",-level-4,marker);
	if (flags & 2)
		printf("%*cContainer inherits ACE\n",-level-4,marker);
	if (flags & 4)
		printf("%*cDon\'t propagate inherits ACE\n",-level-4,marker);
	if (flags & 8)
		printf("%*cInherit only ACE\n",-level-4,marker);
	if (flags & 0x40)
		printf("%*cAudit on success\n",-level-4,marker);
	if (flags & 0x80)
		printf("%*cAudit on failure\n",-level-4,marker);

	printf("%*cSize     0x%x\n",-level,marker,get2l(attr,off+2));

	rights = get4l(attr,off+4);
	printf("%*cAcc rgts 0x%lx\n",-level,marker,(long)rights);
	printf("%*cObj specific acc rgts 0x%lx\n",-level-4,marker,(long)rights & 65535);
	if (isdir) /* a directory */ {
		if (rights & 0x01)
			printf("%*cList directory\n",-level-8,marker);
		if (rights & 0x02)
			printf("%*cAdd file\n",-level-8,marker);
		if (rights & 0x04)
			printf("%*cAdd subdirectory\n",-level-8,marker);
		if (rights & 0x08)
			printf("%*cRead EA\n",-level-8,marker);
		if (rights & 0x10)
			printf("%*cWrite EA\n",-level-8,marker);
		if (rights & 0x20)
			printf("%*cTraverse\n",-level-8,marker);
		if (rights & 0x40)
			printf("%*cDelete child\n",-level-8,marker);
		if (rights & 0x80)
			printf("%*cRead attributes\n",-level-8,marker);
		if (rights & 0x100)
			printf("%*cWrite attributes\n",-level-8,marker);
	}
	else {
	     /* see FILE_READ_DATA etc in winnt.h */
		if (rights & 0x01)
			printf("%*cRead data\n",-level-8,marker);
		if (rights & 0x02)
			printf("%*cWrite data\n",-level-8,marker);
		if (rights & 0x04)
			printf("%*cAppend data\n",-level-8,marker);
		if (rights & 0x08)
			printf("%*cRead EA\n",-level-8,marker);
		if (rights & 0x10)
			printf("%*cWrite EA\n",-level-8,marker);
		if (rights & 0x20)
			printf("%*cExecute\n",-level-8,marker);
		if (rights & 0x80)
			printf("%*cRead attributes\n",-level-8,marker);
		if (rights & 0x100)
			printf("%*cWrite attributes\n",-level-8,marker);
	}
	printf("%*cstandard acc rgts 0x%lx\n",-level-4,marker,(long)(rights >> 16) & 127);
	if (rights & 0x10000)
		printf("%*cDelete\n",-level-8,marker);
	if (rights & 0x20000)
		printf("%*cRead control\n",-level-8,marker);
	if (rights & 0x40000)
		printf("%*cWrite DAC\n",-level-8,marker);
	if (rights & 0x80000)
		printf("%*cWrite owner\n",-level-8,marker);
	if (rights & 0x100000)
		printf("%*cSynchronize\n",-level-8,marker);
	if (rights & 0x800000)
		printf("%*cCan access security ACL\n",-level-4,marker);
	if (rights & 0x10000000)
		printf("%*cGeneric all\n",-level-4,marker);
	if (rights & 0x20000000)
		printf("%*cGeneric execute\n",-level-4,marker);
	if (rights & 0x40000000)
		printf("%*cGeneric write\n",-level-4,marker);
	if (rights & 0x80000000)
		printf("%*cGeneric read\n",-level-4,marker);

	printf("%*cSID at 0x%x\n",-level,marker,off+8);
	showsid(attr,off+8,level+4);
	printf("%*cSummary :",-level,marker);
	if (attr[off] == 0)
		printf(" grant");
	if (attr[off] == 1)
		printf(" deny");
	if (rights & le32_to_cpu(FILE_GREAD | FILE_GWRITE | FILE_GEXEC)) {
		printf(" ");
		if (rights & le32_to_cpu(FILE_GREAD))
			printf("r");
		if (rights & le32_to_cpu(FILE_GWRITE))
			printf("w");
		if (rights & le32_to_cpu(FILE_GEXEC))
			printf("x");
	} else
		printf(" none");
	if (flags & 11)
		printf(" inherited");
	if (!(flags & 8)) {
		int sz;

		printf(" applied");
		sz = attr[off+9]*4 + 8;
		if (!memcmp(&attr[off+8],&attr[get4l(attr,4)],sz))
			printf(" to owner");
		if (!memcmp(&attr[off+8],&attr[get4l(attr,8)],sz))
			printf(" to group");
	}
	printf("\n");

}

void showacl(const char *attr, int off, int isdir, int level)
{
	int i;
	int cnt;
	int size;
	int x;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	size = get2l(attr,off+2);
	printf("%*crevision %d\n",-level,marker,attr[off]);
	printf("%*cACL size %d\n",-level,marker,size);
	cnt = get2l(attr,off+4);
	printf("%*cACE cnt  %d\n",-level,marker,cnt);
	x = 8;
	for (i=0; (i<cnt) && (x < size); i++) {
		printf("%*cACE %d at 0x%x\n",-level,marker,i + 1,off+x);
		showace(attr,off + x,isdir,level+4);
		x += get2l(attr,off + x + 2);
	}
}

void showdacl(const char *attr, int isdir, int level)
{
	int off;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	off = get4l(attr,16);
	if (off) {
		if (level)
			printf("%*c",-level,marker);
		printf("DACL\n");
		showacl(attr,off,isdir,level+4);
	} else {
		if (level)
			printf("%*c",-level,marker);
		printf("No DACL\n");
	}
}

void showsacl(const char *attr, int isdir, int level)
{
	int off;
	char marker;

	if (opt_b)
		marker = '#';
	else
		marker = ' ';
	off = get4l(attr,12);
	if (off) {
		if (level)
			printf("%*c",-level,marker);
		printf("SACL\n");
		showacl(attr,off,isdir,level+4);
	}
	else {
		if (level)
			printf("%*c",-level,marker);
		printf("No SACL\n");
	}
}

void showall(const char *attr, int level)
{
	BOOL isdir;

	isdir = guess_dir(attr);
	showheader(attr,level);
	showusid(attr,level);
	showgsid(attr,level);
	showdacl(attr,isdir,level);
	showsacl(attr,isdir,level);
}

#if POSIXACLS
/*
 *		Display a Posix descriptor
 */

void showposix(const struct POSIX_SECURITY *pxdesc)
{
	char txperm[4];
	const char *txtag;
	const char *txtype;
	const struct POSIX_ACL *acl;
	const struct POSIX_ACE *pxace;
	int acccnt;
	int defcnt;
	int firstdef;
	int perms;
	u16 tag;
	s32 id;
	int k,l;

	if (pxdesc) {
		acccnt = pxdesc->acccnt;
		defcnt = pxdesc->defcnt;
		firstdef = pxdesc->firstdef;
		acl = &pxdesc->acl;
		printf("Posix descriptor :\n");
		printf("    acccnt %d\n",acccnt);
		printf("    defcnt %d\n",defcnt);
		printf("    firstdef %d\n",firstdef);
		printf("    mode : 0%03o\n",(int)pxdesc->mode);
		printf("    tagsset : 0x%02x\n",(int)pxdesc->tagsset);
		printf("Posix ACL :\n");
		printf("    version %d\n",(int)acl->version);
		printf("    flags 0x%02x\n",(int)acl->flags);
		for (k=0; k<(acccnt + defcnt); k++) {
			if (k < acccnt)
				l = k;
			else
				l = firstdef + k - acccnt;
			pxace = &acl->ace[l];
			tag = pxace->tag;
			perms = pxace->perms;
			if (tag == POSIX_ACL_SPECIAL) {
				txperm[0] = (perms & S_ISVTX ? 's' : '-');
				txperm[1] = (perms & S_ISUID ? 'u' : '-');
				txperm[2] = (perms & S_ISGID ? 'g' : '-');
			} else {
				txperm[0] = (perms & 4 ? 'r' : '-');
				txperm[1] = (perms & 2 ? 'w' : '-');
				txperm[2] = (perms & 1 ? 'x' : '-');
			}
			txperm[3] = 0;
			if (k >= acccnt)
				txtype = "default";
			else
				txtype = "access ";
			switch (tag) {
			case POSIX_ACL_USER :
				txtag = "USER ";
				break;
			case POSIX_ACL_USER_OBJ :
				txtag = "USR-O";
				break;
			case POSIX_ACL_GROUP :
				txtag = "GROUP";
				break;
			case POSIX_ACL_GROUP_OBJ :
				txtag = "GRP-O";
				break;
			case POSIX_ACL_MASK :
				txtag = "MASK ";
				break;
			case POSIX_ACL_OTHER :
				txtag = "OTHER";
				break;
			case POSIX_ACL_SPECIAL :
				txtag = "SPECL";
				break;
			default :
				txtag = "UNKWN";
				break;
			}
			id = pxace->id;
			printf("ace %d : %s %s %4ld perms 0%03o %s\n",
				l,txtype,txtag,(long)id,
				perms,txperm);
		}
	} else
		printf("** NULL ACL\n");
}

#endif /* POSIXACLS */

#if defined(WIN32) | defined(STSC)

#else

/*
 *		Relay to get usid as defined during mounting
 */

const SID *relay_find_usid(const struct MAPPING *usermapping __attribute__((unused)),
		uid_t uid, SID *defusid)
{
	return (ntfs_get_usid(ntfs_context,uid,(char*)defusid) ?
			defusid : (SID*)NULL);
}

/*
 *		Relay to get gsid as defined during mounting
 */

const SID *relay_find_gsid(const struct MAPPING *groupmapping __attribute__((unused)),
		uid_t gid, SID *defgsid)
{
	return (ntfs_get_usid(ntfs_context,gid,(char*)defgsid) ?
			defgsid : (SID*)NULL);
}

/*
 *		Relay to get uid as defined during mounting
 */

uid_t relay_find_user(const struct MAPPING *mapping __attribute__((unused)),
			const SID *usid)
{
	int uid;

	uid = ntfs_get_user(ntfs_context,(const char*)usid);
	return (uid < 0 ? 0 : uid);
}

/*
 *		Relay to get gid as defined during mounting
 */

gid_t relay_find_group(const struct MAPPING *mapping __attribute__((unused)),
			const SID *gsid)
{
	int gid;

	gid = ntfs_get_group(ntfs_context,(const char*)gsid);
	return (gid < 0 ? 0 : gid);
}

#endif

#if defined(WIN32) | defined(STSC)

/*
 *		Dummy get uid from user name, out of a Linux environment
 */

struct passwd *getpwnam(const char *user)
{
	ntfs_log_error("Cannot interpret id \"%s\"", user);
	ntfs_log_error("please use numeric uids in UserMapping file\n");
	return ((struct passwd*)NULL);
}

/*
 *		Dummy get gid from group name, out of a Linux environment
 */

struct group *getgrnam(const char *group)
{
	ntfs_log_error("Cannot interpret id \"%s\"", group);
	ntfs_log_error("please use numeric gids in UserMapping file\n");
	return ((struct group*)NULL);
}

#endif /* defined(WIN32) | defined(STSC) */

#if POSIXACLS

struct POSIX_SECURITY *linux_permissions_posix(const char *attr, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
#if OWNERFROMACL
	const SID *osid;
#endif
	const SID *usid;
	const SID *gsid;
	struct POSIX_SECURITY *posix_desc;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
	gsid = (const SID*)&attr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
	osid = (const SID*)&attr[le32_to_cpu(phead->owner)];
	usid = ntfs_acl_owner((const char*)attr);
#if SELFTESTS & !USESTUBS
	if (!opt_t && !ntfs_same_sid(usid,osid))
		printf("== Linux owner is different from Windows owner\n"); 
#else
	if (!ntfs_same_sid(usid,osid))
		printf("== Linux owner is different from Windows owner\n"); 
#endif
#else
	usid = (const SID*)&attr[le32_to_cpu(phead->owner)];
#endif
	posix_desc = ntfs_build_permissions_posix(context.mapping,
			(const char*)attr, usid, gsid, isdir);
	if (!posix_desc) {
		printf("** Failed to build a Posix descriptor\n");
		errors++;
	}
	return (posix_desc);
}

#endif /* POSIXACLS */

int linux_permissions(const char *attr, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
#if OWNERFROMACL
	const SID *osid;
#endif
	const SID *usid;
	const SID *gsid;
	int perm;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
	gsid = (const SID*)&attr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
	osid = (const SID*)&attr[le32_to_cpu(phead->owner)];
	usid = ntfs_acl_owner((const char*)attr);
#if SELFTESTS & !USESTUBS
	if (!opt_t && !ntfs_same_sid(usid,osid))
		printf("== Linux owner is different from Windows owner\n"); 
#else
	if (!ntfs_same_sid(usid,osid))
		printf("== Linux owner is different from Windows owner\n"); 
#endif
#else
	usid = (const SID*)&attr[le32_to_cpu(phead->owner)];
#endif
	perm = ntfs_build_permissions((const char*)attr, usid, gsid, isdir);
	if (perm < 0) {
		printf("** Failed to build permissions\n");
		errors++;
	}
	return (perm);
}

uid_t linux_owner(const char *attr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const SID *usid;
	uid_t uid;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
#if OWNERFROMACL
	usid = ntfs_acl_owner((const char*)attr);
#else
	usid = (const SID*)&attr[le32_to_cpu(phead->owner)];
#endif
#if defined(WIN32) | defined(STSC)
	uid = ntfs_find_user(context.mapping[MAPUSERS],usid);
#else
	if (mappingtype == MAPEXTERN)
		uid = relay_find_user(context.mapping[MAPUSERS],usid);
	else
		uid = ntfs_find_user(context.mapping[MAPUSERS],usid);
#endif
	return (uid);
}

gid_t linux_group(const char *attr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const SID *gsid;
	gid_t gid;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
	gsid = (const SID*)&attr[le32_to_cpu(phead->group)];
#if defined(WIN32) | defined(STSC)
	gid = ntfs_find_group(context.mapping[MAPGROUPS],gsid);
#else
	if (mappingtype == MAPEXTERN)
		gid = relay_find_group(context.mapping[MAPGROUPS],gsid);
	else
		gid = ntfs_find_group(context.mapping[MAPGROUPS],gsid);
#endif
	return (gid);
}

void newblock(s32 key)
{
	struct SECURITY_DATA *psecurdata;
	int i;

	if ((key > 0) && (key < MAXSECURID) && !securdata[key >> SECBLKSZ]) {
		securdata[key >> SECBLKSZ] =
			(struct SECURITY_DATA*)malloc((1 << SECBLKSZ)*sizeof(struct SECURITY_DATA));
		if (securdata[key >> SECBLKSZ])
			for (i=0; i<(1 << SECBLKSZ); i++) {
				psecurdata = &securdata[key >> SECBLKSZ][i];
				psecurdata->filecount = 0;
				psecurdata->mode = 0;
				psecurdata->flags = 0;
				psecurdata->attr = (char*)NULL;
			}
	}
}

void freeblocks(void)
{
	int i,j;
	struct SECURITY_DATA *psecurdata;

	for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
		if (securdata[i]) {
			for (j=0; j<(1 << SECBLKSZ); j++) {
				psecurdata = &securdata[i][j];
				if (psecurdata->attr)
					free(psecurdata->attr);
			}
			free(securdata[i]);
		}
}

/*
 *		Basic read from a user mapping file (Win32)
 */

int basicread(void *fileid, char *buf, size_t size,
		off_t pos __attribute__((unused)))
{
	return (read(*(int*)fileid, buf, size));
}

#if SELFTESTS & !USESTUBS

/*
 *		Read a dummy mapping file for tests
 */

int dummyread(void *fileid  __attribute__((unused)),
		char *buf, size_t size, off_t pos)
{
	size_t sz;

	if (pos >= (off_t)(sizeof(dummymapping) - 1))
		sz = 0;
	else
		if ((size + pos) >= (sizeof(dummymapping) - 1))
			sz = sizeof(dummymapping) - 1 - pos;
		else
			sz = size;
	if (sz > 0)
		memcpy(buf,&dummymapping[pos],sz);
	return (sz);
}

#endif /* POSIXACLS & SELFTESTS & !USESTUBS */

/*
 *		Apply default single user mapping
 *	returns zero if successful
 */

static int do_default_mapping(struct MAPPING *mapping[],
			 const SID *usid)
{
	struct MAPPING *usermapping;
	struct MAPPING *groupmapping;
	SID *sid;
	int sidsz;
	int res;

	res = -1;
	sidsz = ntfs_sid_size(usid);
	sid = (SID*)ntfs_malloc(sidsz);
	if (sid) {
		memcpy(sid,usid,sidsz);
		usermapping = (struct MAPPING*)ntfs_malloc(sizeof(struct MAPPING));
		if (usermapping) {
			groupmapping = (struct MAPPING*)ntfs_malloc(sizeof(struct MAPPING));
			if (groupmapping) {
				usermapping->sid = sid;
				usermapping->xid = 0;
				usermapping->next = (struct MAPPING*)NULL;
				groupmapping->sid = sid;
				groupmapping->xid = 0;
				groupmapping->next = (struct MAPPING*)NULL;
				mapping[MAPUSERS] = usermapping;
				mapping[MAPGROUPS] = groupmapping;
				res = 0;
			}
		}
	}
	return (res);
}

/*
 *		Build the user mapping
 *	- according to a mapping file if defined (or default present),
 *	- or try default single user mapping if possible
 *
 *	The mapping is specific to a mounted device
 *	No locking done, mounting assumed non multithreaded
 *
 *	returns zero if mapping is successful
 *	(failure should not be interpreted as an error)
 */

int local_build_mapping(struct MAPPING *mapping[], const char *usermap_path)
{
#ifdef WIN32
	char mapfile[sizeof(MAPDIR) + sizeof(MAPFILE) + 6];
#else
	char *mapfile;
	char *p;
#endif
	int fd;
	struct MAPLIST *item;
	struct MAPLIST *firstitem = (struct MAPLIST*)NULL;
	struct MAPPING *usermapping;
	struct MAPPING *groupmapping;
	static struct {
		u8 revision;
		u8 levels;
		be16 highbase;
		be32 lowbase;
		le32 level1;
		le32 level2;
		le32 level3;
		le32 level4;
		le32 level5;
	} defmap = {
		1, 5, const_cpu_to_be16(0), const_cpu_to_be32(5),
		const_cpu_to_le32(21),
		const_cpu_to_le32(DEFSECAUTH1), const_cpu_to_le32(DEFSECAUTH2),
		const_cpu_to_le32(DEFSECAUTH3), const_cpu_to_le32(DEFSECBASE)
	} ;

	/* be sure not to map anything until done */
	mapping[MAPUSERS] = (struct MAPPING*)NULL;
	mapping[MAPGROUPS] = (struct MAPPING*)NULL;

	if (usermap_path) {
#ifdef WIN32
/* TODO : check whether the device can store acls */
		strcpy(mapfile,"x:\\" MAPDIR "\\" MAPFILE);
		if (((le16*)usermap_path)[1] == ':')
  			mapfile[0] = usermap_path[0];
		else
			mapfile[0] = getdrive() + 'A' - 1;
		fd = open(mapfile,O_RDONLY);
#else
		fd = 0;
		mapfile = (char*)malloc(MAXFILENAME);
		if (mapfile) {
			/* build a full path to locate the mapping file */
			if ((usermap_path[0] != '/')
			   && getcwd(mapfile,MAXFILENAME)) {
				strcat(mapfile,"/");
				strcat(mapfile,usermap_path);
			} else
				strcpy(mapfile,usermap_path);
			p = strrchr(mapfile,'/');
			if (p)
				do {
					strcpy(p,"/" MAPDIR "/" MAPFILE);
					fd = open(mapfile,O_RDONLY);
					if (fd <= 0) {
						*p = 0;
						p = strrchr(mapfile,'/');
						if (p == mapfile)
							p = (char*)NULL;
					}
				} while ((fd <= 0) && p);
			free(mapfile);
			if (!p) {
				printf("** Could not find the user mapping file\n");
				if (usermap_path[0] != '/')
					printf("   Retry with full path of file\n");
				errors++;
			}
		}
#endif
		if (fd > 0) {
			firstitem = ntfs_read_mapping(basicread, (void*)&fd);
			close(fd);
		}
	} else {
#if SELFTESTS & !USESTUBS
		firstitem = ntfs_read_mapping(dummyread, (void*)NULL);
#endif
	}

	if (firstitem) {
		usermapping = ntfs_do_user_mapping(firstitem);
		groupmapping = ntfs_do_group_mapping(firstitem);
		if (usermapping && groupmapping) {
			mapping[MAPUSERS] = usermapping;
			mapping[MAPGROUPS] = groupmapping;
		} else
			ntfs_log_error("There were no valid user or no valid group\n");
		/* now we can free the memory copy of input text */
		/* and rely on internal representation */
		while (firstitem) {
			item = firstitem->next;
#if USESTUBS
			stdfree(firstitem); /* allocated within library */
#else
			free(firstitem);
#endif
			firstitem = item;
		}
	} else {
		do_default_mapping(mapping,(const SID*)&defmap);
	}
	if (mapping[MAPUSERS])
		mappingtype = MAPLOCAL;
	return (!mapping[MAPUSERS]);
}

/*
 *		Get an hexadecimal number (source with MSB first)
 */

u32 getmsbhex(const char *text)
{
	u32 v;
	int b;
	BOOL ok;

	v = 0;
	ok = TRUE;
	do {
		b = *text++;
		if ((b >= '0') && (b <= '9'))
			v = (v << 4) + b - '0';
		else
			if ((b >= 'a') && (b <= 'f'))
				v = (v << 4) + b - 'a' + 10;
			else
				if ((b >= 'A') && (b <= 'F'))
					v = (v << 4) + b - 'A' + 10;
				else ok = FALSE;
	} while (ok);
	return (v);
}


/*
 *		Get an hexadecimal number (source with LSB first)
 *	An odd number of digits might yield a strange result
 */

u32 getlsbhex(const char *text)
{
	u32 v;
	int b;
	BOOL ok;
	int pos;

	v = 0;
	ok = TRUE;
	pos = 0;
	do {
		b = *text++;
		if ((b >= '0') && (b <= '9'))
			v |= (u32)(b - '0') << (pos ^ 4);
		else
			if ((b >= 'a') && (b <= 'f'))
				v |= (u32)(b - 'a' + 10) << (pos ^ 4);
			else
				if ((b >= 'A') && (b <= 'F'))
					v |= (u32)(b - 'A' + 10) << (pos ^ 4);
				else ok = FALSE;
		pos += 4;
	} while (ok);
	return (v);
}


/*
 *		Check whether a line looks like an hex dump
 */

BOOL ishexdump(const char *line, int first, int lth)
{
	BOOL ok;
	int i;
	int b;

	ok = (first >= 0) && (lth >= (first + 16));
	for (i=0; ((first+i)<lth) && ok; i++) {
		b = line[first + i];
		if ((i == 6)
		    || (i == 7)
		    || (i == 16)
		    || (i == 25)
		    || (i == 34)
		    || (i == 43))
			ok = (b == ' ') || (b == '\n');
		else
			ok = ((b >= '0') && (b <= '9'))
			    || ((b >= 'a') && (b <= 'f'))
			    || ((b >= 'A') && (b <= 'F'));
	}
	return (ok);
}


/*
 *		Display security descriptors from a file
 *	This is typically to convert a verbose output to a very verbose one
 */

void showhex(FILE *fd)
{
	static char attr[MAXATTRSZ];
	char line[MAXLINE+1];
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif
	int lth;
	int first;
	unsigned int pos;
	unsigned char b;
	u32 v;
	int c;
	int isdir;
	int mode;
	unsigned int off;
	int i;
	BOOL isdump;
	BOOL done;

	b = 0;
	pos = 0;
	off = 0;
	done = FALSE;
	do {
			/* input a (partial) line without displaying */
		lth = 0;
		first = -1;
		do {
			c = getc(fd);
			if ((c != ' ') && (first < 0))
				first = lth;
			if (c == EOF)
				done = TRUE;
			else
				if (c != '\r')
					line[lth++] = c;
		} while (!done && (c != '\n') && (lth < MAXLINE));
			/* check whether this looks like an hexadecimal dump */
		isdump = ishexdump(line, first, lth);
		if (isdump) off = getmsbhex(&line[first]);
			/* line is not an hexadecimal dump */
			/* display what we have in store */
		if ((!isdump || !off) && pos && ntfs_valid_descr((char*)attr,pos)) {
			printf("	Computed hash : 0x%08lx\n",
				    (unsigned long)hash((le32*)attr,
				    ntfs_attr_size(attr)));
			isdir = guess_dir(attr);
			printf("    Estimated type : %s\n",(isdir ? "directory" : "file"));
			showheader(attr,4);
			showusid(attr,4);
			showgsid(attr,4);
			showdacl(attr,isdir,4);
			showsacl(attr,isdir,4);
			mode = linux_permissions(attr,isdir);
			printf("Interpreted Unix mode 0%03o\n",mode);
#if POSIXACLS
				/*
				 * Posix display not possible when user
				 * mapping is not available (option -h)
				 */
			if (mappingtype != MAPNONE) {
				pxdesc = linux_permissions_posix(attr,isdir);
				if (pxdesc) {
					showposix(pxdesc);
					free(pxdesc);
				}
			}
#endif
			pos = 0;
		}
		if (isdump && !off)
			pos = off;
			/* line looks like an hexadecimal dump */
			/* decode it into attribute */
		if (isdump && (off == pos)) {
			for (i=first+8; i<lth; i+=9) {
				v = getlsbhex(&line[i]);
				*(le32*)&attr[pos] = cpu_to_le32(v);
				pos += 4;
			}
		}
			/* display (full) current line */
		if (lth) printf("! ");
		for (i=0; i<lth; i++) {
			c = line[i];
			putchar(c);
		}
		while (!done && (c != '\n')) {
			c = getc(fd);
			if (c == EOF)
				done = TRUE;
			else
				putchar(c);
		}
	} while (!done);
}

BOOL applyattr(const char *fullname, const char *attr,
			BOOL withattr, int attrib, s32 key)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	struct SECURITY_DATA *psecurdata;
	const char *curattr;
	char *newattr;
	int selection;
	BOOL bad;
	BOOL badattrib;
	BOOL err;

	err = FALSE;
	psecurdata = (struct SECURITY_DATA*)NULL;
	curattr = (const char*)NULL;
	newattr = (char*)NULL;
	if ((key > 0) && (key < MAXSECURID)) {
		if (!securdata[key >> SECBLKSZ])
			newblock(key);
		if (securdata[key >> SECBLKSZ]) {
			psecurdata = &securdata[key >> SECBLKSZ]
					[key & ((1 << SECBLKSZ) - 1)];
		}
	}

			/* If we have a usable attrib value. Try applying */
	badattrib = FALSE;
	if (opt_e && (attrib != INVALID_FILE_ATTRIBUTES)) {
#ifdef WIN32
		badattrib = !SetFileAttributesW((LPCWSTR)fullname, attrib);
#else
		badattrib = !ntfs_set_file_attributes(ntfs_context, fullname, attrib);
#endif
		if (badattrib) {
			printf("** Could not set Windows attrib of ");
			printname(stdout,fullname);
			printf(" to 0x%x\n", attrib);
			printerror(stdout);
			warnings++;
		}
	}

	if (withattr) {
		if (psecurdata) {
			newattr = (char*)malloc(ntfs_attr_size(attr));
			if (newattr) {
				memcpy(newattr,attr,ntfs_attr_size(attr));
				psecurdata->attr = newattr;
			}
		}
		curattr = attr;
	} else
		/*
		 * No explicit attr in backup, use attr defined
		 * previously for the same id
		 */
		if (psecurdata)
			curattr = psecurdata->attr;
       

	if (curattr) {
		phead = (const SECURITY_DESCRIPTOR_RELATIVE*)curattr;
#ifdef WIN32
			/* SACL currently not set, need some special privilege */
		selection = OWNER_SECURITY_INFORMATION
			| GROUP_SECURITY_INFORMATION
			| DACL_SECURITY_INFORMATION;
		bad = !SetFileSecurityW((LPCWSTR)fullname,
			selection, (char*)curattr);
		if (bad)
			switch (GetLastError()) {
			case 1307 :
			case 1314 :
				printf("** Could not set owner of ");
				printname(stdout,fullname);
				printf(", retrying with no owner setting\n");
				warnings++;
				bad = !SetFileSecurityW((LPCWSTR)fullname,
					selection & ~OWNER_SECURITY_INFORMATION, (char*)curattr);
				break;
			default :
				break;
			}
#else
		selection = OWNER_SECURITY_INFORMATION
			| GROUP_SECURITY_INFORMATION
			| DACL_SECURITY_INFORMATION
			| SACL_SECURITY_INFORMATION;
		bad = !ntfs_set_file_security(ntfs_context,fullname,
			selection, (const char*)curattr);
#endif
		if (bad) {
			printf("** Could not set the ACL of ");
			printname(stdout,fullname);
			printf("\n");
			printerror(stdout);
			err = TRUE;
		} else
			if (opt_v) {
				if (opt_e && !badattrib)
					printf("ACL and attrib have been applied to ");
				else
					printf("ACL has been applied to ");
				printname(stdout,fullname);
				printf("\n");

			}
	} else {
		printf("** There was no valid ACL for ");
		printname(stdout,fullname);
		printf("\n");
		err = TRUE;
	}
	return (!err);
}

/*
 *		Restore security descriptors from a file
 */

BOOL restore(FILE *fd)
{
	static char attr[MAXATTRSZ];
	char line[MAXFILENAME+25];
	char fullname[MAXFILENAME+25];
	SECURITY_DESCRIPTOR_RELATIVE *phead;
	int lth;
	int first;
	unsigned int pos;
	unsigned int size;
	unsigned char b;
	int c;
	int isdir;
	int mode;
	s32 key;
	BOOL isdump;
	unsigned int off;
	u32 v;
	u32 oldhash;
	int i;
	int count;
	int attrib;
	BOOL withattr;
	BOOL done;

	b = 0;
	pos = 0;
	size = 0;
	off = 0;
	done = FALSE;
	withattr = FALSE;
	oldhash = 0;
	key = 0;
	errors = 0;
	count = 0;
	fullname[0] = 0;
	attrib = INVALID_FILE_ATTRIBUTES;
	do {
			/* input a (partial) line without processing */
		lth = 0;
		first = -1;
		do {
			c = getc(fd);
			if ((c != ' ') && (first < 0))
				first = lth;
			if (c == EOF)
				done = TRUE;
			else
				if (c != '\r')
					line[lth++] = c;
		} while (!done && (c != '\n') && (lth < (MAXFILENAME + 24)));
			/* check whether this looks like an hexadecimal dump */
		isdump = ishexdump(line, first, lth);
		if (isdump) off = getmsbhex(&line[first]);
			/* line is not an hexadecimal dump */
			/* apply what we have in store */
		if ((!isdump || !off) && pos && ntfs_valid_descr((char*)attr,pos)) {
			withattr = TRUE;
			if (opt_v >= 2) {
				printf("	Computed hash : 0x%08lx\n",
					    (unsigned long)hash((le32*)attr,
					    ntfs_attr_size(attr)));
				isdir = guess_dir(attr);
				printf("    Estimated type : %s\n",(isdir ? "directory" : "file"));
				showheader(attr,4);
				showusid(attr,4);
				showgsid(attr,4);
				showdacl(attr,isdir,4);
				showsacl(attr,isdir,4);
				mode = linux_permissions(attr,isdir);
				printf("Interpreted Unix mode 0%03o\n",mode);
			}
			size = pos;
			pos = 0;
		}
		if (isdump && !off)
			pos = off;
			/* line looks like an hexadecimal dump */
			/* decode it into attribute */
		if (isdump && (off == pos)) {
			for (i=first+8; i<lth; i+=9) {
				v = getlsbhex(&line[i]);
				*(le32*)&attr[pos] = cpu_to_le32(v);
				pos += 4;
			}
		}
			/* display (full) current line unless dump or verbose */
		if (!isdump || opt_v) {
			if(lth) printf("! ");
			for (i=0; i<lth; i++) {
				c = line[i];
				putchar(c);
			}
		}
		while (!done && (c != '\n')) {
			c = getc(fd);
			if (c == EOF)
				done = TRUE;
			else
				if (!isdump || opt_v)
					putchar(c);
		}

		line[lth] = 0;
		while ((lth > 0)
		    && ((line[lth-1] == '\n') || (line[lth-1] == '\r')))
			line[--lth] = 0;
		if (!strncmp(line,"Computed hash : 0x",18))
			oldhash = getmsbhex(&line[18]);
		if (!strncmp(line,"Security key : 0x",17))
			key = getmsbhex(&line[17]);
		if (!strncmp(line,"Windows attrib : 0x",19))
			attrib = getmsbhex(&line[19]);
		if (done
		    || !strncmp(line,"File ",5)
		    || !strncmp(line,"Directory ",10)) {
			/*
			 *  New file or directory (or end of file) :
			 *  apply attribute just collected
			 *  or apply attribute defined from current key
			 */

			if (withattr
			    && oldhash
			    && (hash((const le32*)attr,ntfs_attr_size(attr)) != oldhash)) {
				printf("** ACL rejected, its hash is not as expected\n");
				errors++;
			} else
				if (fullname[0]) {
					phead = (SECURITY_DESCRIPTOR_RELATIVE*)attr;
					/* set the request for auto-inheritance */
					if (phead->control & SE_DACL_AUTO_INHERITED)
						phead->control |= SE_DACL_AUTO_INHERIT_REQ;
					if (!applyattr(fullname,attr,withattr,
							attrib,key))
						errors++;
					else
						count++;
				}
			/* save current file or directory name */
			withattr = FALSE;
			key = 0;
			oldhash = 0;
			attrib = INVALID_FILE_ATTRIBUTES;
			if (!done) {
#ifdef WIN32
				if (!strncmp(line,"File ",5))
					makeutf16(fullname,&line[5]);
				else
					makeutf16(fullname,&line[10]);
#else
				if (!strncmp(line,"File ",5))
					strcpy(fullname,&line[5]);
				else
					strcpy(fullname,&line[10]);
#endif
			}
		}
	} while (!done);
	printf("%d ACLs have been applied\n",count);
	return (FALSE);
}

/*
 *		Open the security API in rw mode for an ACL restoration
 */

#ifdef WIN32
#else
BOOL dorestore(const char *volume, FILE *fd)
{
	BOOL err;

	err = FALSE;
	if (!getuid()) {
 		if (open_security_api()) {
			if (open_volume(volume,MS_NONE)) {
				if (restore(fd)) err = TRUE;
				close_volume(volume);
			} else {
				fprintf(stderr,"Could not open volume %s\n",volume);
				printerror(stderr);
				err = TRUE;
			}
			close_security_api();
		} else {
			fprintf(stderr,"Could not open security API\n");
			printerror(stderr);
			err = TRUE;
		}
	} else {
		fprintf(stderr,"Restore is only possible as root\n");
		err = TRUE;
	}
	return (err);
}
#endif /* WIN32 */

#if POSIXACLS & SELFTESTS & !USESTUBS

/*
 *		Merge Posix ACL rights into an u32 (self test only)
 *
 *	Result format : -----rwxrwxrwxrwxrwx---rwxrwxrwx
 *                           U1 U2 G1 G2  M     o  g  w
 *
 *	Only two users (U1, U2) and two groups (G1, G2) taken into account
 */
u32 merge_rights(const struct POSIX_SECURITY *pxdesc, BOOL def)
{
	const struct POSIX_ACE *pxace;
	int i;
	int users;
	int groups;
	int first;
	int last;
	u32 rights;

	rights = 0;
	users = 0;
	groups = 0;
	if (def) {
		first = pxdesc->firstdef;
		last = pxdesc->firstdef + pxdesc->defcnt - 1;
	} else {
		first = 0;
		last = pxdesc->acccnt - 1;
	}
	pxace = pxdesc->acl.ace;
	for (i=first; i<=last; i++) {
		switch (pxace[i].tag) {
		case POSIX_ACL_USER_OBJ :
			rights |= (pxace[i].perms & 7) << 6;
			break;
		case POSIX_ACL_USER :
			if (users < 2)
				rights |= ((u32)pxace[i].perms & 7) << (24 - 3*users);
			users++;
			break;
		case POSIX_ACL_GROUP_OBJ :
			rights |= (pxace[i].perms & 7) << 3;
			break;
		case POSIX_ACL_GROUP :
			if (groups < 2)
				rights |= ((u32)pxace[i].perms & 7) << (18 - 3*groups);
			groups++;
			break;
		case POSIX_ACL_MASK :
			rights |= ((u32)pxace[i].perms & 7) << 12;
			break;
		case POSIX_ACL_OTHER :
			rights |= (pxace[i].perms & 7);
			break;
		default :
			break;
		}
	}
	return (rights);
}

void tryposix(struct POSIX_SECURITY *pxdesc)
{
	le32 owner_sid[] = /* S-1-5-21-3141592653-589793238-462843383-1016 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(1016)
		} ;
	le32 group_sid[] = /* S-1-5-21-3141592653-589793238-462843383-513 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(513)
		} ;

	char *attr;
	BOOL isdir;
	mode_t mode;
	struct POSIX_SECURITY *newpxdesc;
	struct POSIX_SECURITY *oldpxdesc;
	static char *oldattr = (char*)NULL;

	isdir = FALSE;
	if (oldattr) {
		oldpxdesc = linux_permissions_posix(oldattr, isdir);
		newpxdesc = ntfs_merge_descr_posix(pxdesc, oldpxdesc);
		if (!newpxdesc)
			newpxdesc = pxdesc;
		free(oldpxdesc);
		if (opt_v) {
			printf("merged descriptors :\n");
			showposix(newpxdesc);
		}
	} else
		newpxdesc = pxdesc;
	attr = ntfs_build_descr_posix(context.mapping,newpxdesc,
			isdir,(SID*)owner_sid,(SID*)group_sid);
	if (attr && ntfs_valid_descr(attr, ntfs_attr_size(attr))) {
		if (opt_v)
			hexdump(attr,ntfs_attr_size(attr),8);
		if (opt_v >= 2) {
			showheader(attr,4);
			showusid(attr,4);
			showgsid(attr,4);
			showdacl(attr,isdir,4);
			showsacl(attr,isdir,4);
			mode = linux_permissions(attr,isdir);
			printf("Interpreted Unix mode 0%03o\n",mode);
			printf("Interpreted back Posix descriptor :\n");
			newpxdesc = linux_permissions_posix(attr,isdir);
			showposix(newpxdesc);
			free(newpxdesc);
		}
		if (oldattr) free(oldattr);
		oldattr = attr;
	}
}

static BOOL same_posix(struct POSIX_SECURITY *pxdesc1,
			struct POSIX_SECURITY *pxdesc2)
{
	BOOL same;
	int i;

	same = pxdesc1
		&& pxdesc2
		&& (pxdesc1->mode == pxdesc2->mode)
		&& (pxdesc1->acccnt == pxdesc2->acccnt)
		&& (pxdesc1->defcnt == pxdesc2->defcnt)
		&& (pxdesc1->firstdef == pxdesc2->firstdef)
		&& (pxdesc1->tagsset == pxdesc2->tagsset)
		&& (pxdesc1->acl.version == pxdesc2->acl.version)
		&& (pxdesc1->acl.flags == pxdesc2->acl.flags);
	i = 0;
	while (same && (i < pxdesc1->acccnt)) {
		same = (pxdesc1->acl.ace[i].tag == pxdesc2->acl.ace[i].tag)
		   && (pxdesc1->acl.ace[i].perms == pxdesc2->acl.ace[i].perms)
		   && (pxdesc1->acl.ace[i].id == pxdesc2->acl.ace[i].id);
		i++;
	}
	i = pxdesc1->firstdef;
	while (same && (i < pxdesc1->firstdef + pxdesc1->defcnt)) {
		same = (pxdesc1->acl.ace[i].tag == pxdesc2->acl.ace[i].tag)
		   && (pxdesc1->acl.ace[i].perms == pxdesc2->acl.ace[i].perms)
		   && (pxdesc1->acl.ace[i].id == pxdesc2->acl.ace[i].id);
		i++;
	}
	return (same);
}

#endif /* POSIXACLS & SELFTESTS & !USESTUBS */

#if SELFTESTS & !USESTUBS

/*
 *		Build a dummy security descriptor
 *	returns descriptor in allocated memory, must free() after use
 */

static char *build_dummy_descr(BOOL isdir __attribute__((unused)),
			const SID *usid, const SID *gsid,
			int cnt, 
			 /* seq of int allow, SID *sid, int flags, u32 mask */
			...)
{
	char *attr;
	int attrsz;
	SECURITY_DESCRIPTOR_RELATIVE *pnhead;
	ACL *pacl;
	ACCESS_ALLOWED_ACE *pace;
	va_list ap;
	const SID *sid;
	u32 umask;
	le32 mask;
	int flags;
	BOOL allow;
	int pos;
	int usidsz;
	int gsidsz;
	int sidsz;
	int aclsz;
	int i;

	if (usid)
		usidsz = ntfs_sid_size(usid);
	else
		usidsz = 0;
	if (gsid)
		gsidsz = ntfs_sid_size(gsid);
	else
		gsidsz = 0;


	/* allocate enough space for the new security attribute */
	attrsz = sizeof(SECURITY_DESCRIPTOR_RELATIVE)	/* header */
	    + usidsz + gsidsz	/* usid and gsid */
	    + sizeof(ACL)	/* acl header */
	    + cnt*40;

	attr = (char*)ntfs_malloc(attrsz);
	if (attr) {
		/* build the main header part */
		pnhead = (SECURITY_DESCRIPTOR_RELATIVE*) attr;
		pnhead->revision = SECURITY_DESCRIPTOR_REVISION;
		pnhead->alignment = 0;
			/*
			 * The flag SE_DACL_PROTECTED prevents the ACL
			 * to be changed in an inheritance after creation
			 */
		pnhead->control = SE_DACL_PRESENT | SE_DACL_PROTECTED
				    | SE_SELF_RELATIVE;
			/*
			 * Windows prefers ACL first, do the same to
			 * get the same hash value and avoid duplication
			 */
		/* build the ACL header */
		pos = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
		pacl = (ACL*)&attr[pos];
		pacl->revision = ACL_REVISION;
		pacl->alignment1 = 0;
		pacl->size = cpu_to_le16(0); /* fixed later */
		pacl->ace_count = cpu_to_le16(cnt);
		pacl->alignment2 = cpu_to_le16(0);

		/* enter the ACEs */

		pos += sizeof(ACL);
		aclsz = sizeof(ACL);
		va_start(ap,cnt);
		for (i=0; i<cnt; i++) {
			pace = (ACCESS_ALLOWED_ACE*)&attr[pos];
			allow = va_arg(ap,int);
			sid = va_arg(ap,SID*);
			flags = va_arg(ap,int);
			umask = va_arg(ap,u32);
			mask = cpu_to_le32(umask);
			sidsz = ntfs_sid_size(sid);
			pace->type = (allow ? ACCESS_ALLOWED_ACE_TYPE : ACCESS_DENIED_ACE_TYPE);
			pace->flags = flags;
			pace->size = cpu_to_le16(sidsz + 8);
			pace->mask = mask;
			memcpy(&pace->sid,sid,sidsz);
			aclsz += sidsz + 8;
			pos += sidsz + 8;
		}
		va_end(ap);

		/* append usid and gsid if defined */
		/* positions of ACL, USID and GSID into header */
		pnhead->owner = cpu_to_le32(0);
		pnhead->group = cpu_to_le32(0);
		if (usid) {
			memcpy(&attr[pos], usid, usidsz);
			pnhead->owner = cpu_to_le32(pos);
		}
		if (gsid) {
			memcpy(&attr[pos + usidsz], gsid, gsidsz);
			pnhead->group = cpu_to_le32(pos + usidsz);
		}
		/* positions of DACL and SACL into header */
		pnhead->sacl = cpu_to_le32(0);
		if (cnt) {
			pacl->size = cpu_to_le16(aclsz);
			pnhead->dacl =
			    cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE));
		} else
			pnhead->dacl = cpu_to_le32(0);
		if (!ntfs_valid_descr(attr,pos+usidsz+gsidsz)) {
			printf("** Bad sample descriptor\n");
			free(attr);
			attr = (char*)NULL;
			errors++;
		}
	} else
		errno = ENOMEM;
	return (attr);
}

/*
 *		Check a few samples with special conditions
 */

void check_samples()
{
	char *descr = (char*)NULL;
	BOOL isdir = FALSE;
	mode_t perms;
	mode_t expect = 0;
	int cnt;
	u32 expectacc;
	u32 expectdef;
#if POSIXACLS
	u32 accrights;
	u32 defrights;
	mode_t mixmode;
	struct POSIX_SECURITY *pxdesc;
	struct POSIX_SECURITY *pxsample;
	const char *txsample;
#endif
	le32 owner1[] = /* S-1-5-21-1833069642-4243175381-1340018762-1003 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(1833069642), cpu_to_le32(4243175381),
		cpu_to_le32(1340018762), cpu_to_le32(1003)
		} ;
	le32 group1[] = /* S-1-5-21-1833069642-4243175381-1340018762-513 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(1833069642), cpu_to_le32(4243175381),
		cpu_to_le32(1340018762), cpu_to_le32(513)
		} ;
	le32 group2[] = /* S-1-5-21-1607551490-981732888-1819828000-513 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(1607551490), cpu_to_le32(981732888),
		cpu_to_le32(1819828000), cpu_to_le32(513)
		} ;
	le32 owner3[] = /* S-1-5-21-3141592653-589793238-462843383-1016 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(1016)
		} ;
	le32 group3[] = /* S-1-5-21-3141592653-589793238-462843383-513 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(513)
		} ;

#if POSIXACLS
	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[4];
	} sampletry1 =
	{
		{ 0645, 4, 0, 4, 0x35,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 6, -1 },
			{ 4, 5, -1 },
			{ 16, 4, -1 },
			{ 32, 5, -1 }
		}
	} ;

	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[6];
	} sampletry3 =
	{
		{ 0100, 6, 0, 6, 0x3f,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 1, -1 },
			{ 2, 3, 1000 },
			{ 4, 1, -1 },
			{ 8, 3, 1002 },
			{ 16, 0, -1 },
			{ 32, 0, -1 }
		}
	} ;

	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[8];
	} sampletry4 =
	{
		{ 0140, 8, 0, 8, 0x3f,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 1, -1 },
			{ 2, 3, 516 },
			{ 2, 6, 1000 },
			{ 4, 1, -1 },
			{ 8, 6, 500 },
			{ 8, 3, 1002 },
			{ 16, 4, -1 },
			{ 32, 0, -1 }
		}
	} ;

	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[6];
	} sampletry5 =
	{
		{ 0454, 6, 0, 6, 0x3f,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 4, -1 },
			{ 2, 5, 516 },
			{ 4, 4, -1 },
			{ 8, 6, 500 },
			{ 16, 5, -1 },
			{ 32, 4, -1 }
		}
	} ;

	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[8];
	} sampletry6 =
	{
		{ 0332, 8, 0, 8, 0x3f,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 3, -1 },
			{ 2, 1,  0 },
			{ 2, 2,  1000 },
			{ 4, 6, -1 },
			{ 8, 4,  0 },
			{ 8, 5,  1002 },
			{ 16, 3, -1 },
			{ 32, 2, -1 }
		}
	} ;

	struct {
		struct POSIX_SECURITY head;
		struct POSIX_ACE ace[4];
	} sampletry8 =
	{
		{ 0677, 4, 0, 4, 0x35,
			{ POSIX_VERSION, 0, 0 }
		},
		{
			{ 1, 6, -1 },
			{ 4, 7, -1 },
			{ 16, 7, -1 },
			{ 32, 7, -1 }
		}
	} ;

#endif /* POSIXACLS */


#if POSIXACLS
	for (cnt=1; cnt<=8; cnt++) {
		switch (cnt) {
		case 1 :
			pxsample = &sampletry1.head;
			txsample = "sampletry1-a";
			isdir = FALSE;
			descr = ntfs_build_descr_posix(context.mapping,&sampletry1.head,
				isdir, (const SID*)owner3, (const SID*)group3);
			break;
		case 2 :
			pxsample = &sampletry1.head;
			txsample = "sampletry1-b";
			isdir = FALSE;
			descr = ntfs_build_descr_posix(context.mapping,&sampletry1.head,
				isdir, (const SID*)adminsid, (const SID*)group3);
			break;
		case 3 :
			isdir = FALSE;
			pxsample = &sampletry3.head;
			txsample = "sampletry3";
			descr = ntfs_build_descr_posix(context.mapping,pxsample,
				isdir, (const SID*)group3, (const SID*)group3);
			break;
		case 4 :
			isdir = FALSE;
			pxsample = &sampletry4.head;
			txsample = "sampletry4";
			descr = ntfs_build_descr_posix(context.mapping,pxsample,
				isdir, (const SID*)owner3, (const SID*)group3);
			break;
		case 5 :
			isdir = FALSE;
			pxsample = &sampletry5.head;
			txsample = "sampletry5";
			descr = ntfs_build_descr_posix(context.mapping,pxsample,
				isdir, (const SID*)owner3, (const SID*)group3);
			break;
		case 6 :
			isdir = FALSE;
			pxsample = &sampletry6.head;
			txsample = "sampletry6-a";
			descr = ntfs_build_descr_posix(context.mapping,pxsample,
				isdir, (const SID*)owner3, (const SID*)group3);
			break;
		case 7 :
			isdir = FALSE;
			pxsample = &sampletry6.head;
			txsample = "sampletry6-b";
			descr = ntfs_build_descr_posix(context.mapping,pxsample,
				isdir, (const SID*)adminsid, (const SID*)adminsid);
			break;
		case 8 :
			pxsample = &sampletry8.head;
			txsample = "sampletry8";
			isdir = FALSE;
			descr = ntfs_build_descr_posix(context.mapping,&sampletry8.head,
				isdir, (const SID*)owner3, (const SID*)group3);
			break;
		default :
			pxsample = (struct POSIX_SECURITY*)NULL;
			txsample = (const char*)NULL;
		}
				/* check we get original back */
		if (descr)
			pxdesc = linux_permissions_posix(descr, isdir);
		else
			pxdesc = (struct POSIX_SECURITY*)NULL;
		if (!descr || !pxdesc || !same_posix(pxsample,pxdesc)) {
			printf("** Error in %s\n",txsample);
			showposix(pxsample);
			showall(descr,0);
			showposix(pxdesc);
			errors++;
		}
		free(descr);
		free(pxdesc);
	}

#endif /* POSIXACLS */


		/*
		 *		Check a few samples built by Windows,
		 *	which cannot be generated by Linux
		 */

	for (cnt=1; cnt<=8; cnt++) {
		switch(cnt) {
		case 1 :  /* hp/tmp */
			isdir = TRUE;
			descr = build_dummy_descr(isdir,
				(const SID*)owner1, (const SID*)group1,
				1,
				(int)TRUE, worldsid, (int)0x3, (u32)0x1f01ff);
			expectacc = expect = 0777;
			expectdef = 0;
			break;
		case 2 :  /* swsetup */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, adminsid, (const SID*)group2,
				2,
				(int)TRUE, worldsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, worldsid, (int)0xb, (u32)0x1f01ff);
			expectacc = expect = 0777;
			expectdef = 0777;
			break;
		case 3 :  /* Dr Watson */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, (const SID*)owner3, (const SID*)group3,
				0);
			expectacc = expect = 0700;
			expectdef = 0;
			break;
		case 4 :
			isdir = FALSE;
			descr = build_dummy_descr(isdir,
				(const SID*)owner3, (const SID*)group3,
				4,
				(int)TRUE, (const SID*)owner3, 0, 
					le32_to_cpu(FILE_READ_DATA | OWNER_RIGHTS),
				(int)TRUE, (const SID*)group3, 0,
					le32_to_cpu(FILE_WRITE_DATA),
				(int)TRUE, (const SID*)group2, 0,
					le32_to_cpu(FILE_WRITE_DATA | FILE_READ_DATA),
				(int)TRUE, (const SID*)worldsid, 0,
					le32_to_cpu(FILE_EXECUTE));
			expect = 0731;
			expectacc = 07070731;
			expectdef = 0;
			break;
		case 5 :  /* Vista/JP */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, systemsid, systemsid,
				6,
				(int)TRUE, owner1, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, systemsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, adminsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, owner1, (int)0xb, (u32)0x10000000,
				(int)TRUE, systemsid, (int)0xb, (u32)0x10000000,
				(int)TRUE, adminsid, (int)0xb, (u32)0x10000000);
			expectacc = expect = 0700;
			expectdef = 0700;
			break;
		case 6 :  /* Vista/JP2 */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, systemsid, systemsid,
				7,
				(int)TRUE, owner1,    (int)0x0, (u32)0x1f01ff,
				(int)TRUE, systemsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, adminsid,  (int)0x0, (u32)0x1f01ff,
				(int)TRUE, owner1,    (int)0xb, (u32)0x1f01ff,
				(int)TRUE, systemsid, (int)0xb, (u32)0x1f01ff,
				(int)TRUE, adminsid,  (int)0xb, (u32)0x1f01ff,
				(int)TRUE, owner3,    (int)0x3, (u32)0x1200a9);
			expectacc = 0500070700;
			expectdef = 0700;
			expect = 0700;
			break;
		case 7 :  /* WinXP/JP */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, adminsid, systemsid,
				6,
				(int)TRUE, owner1, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, systemsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, adminsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, owner1, (int)0xb, (u32)0x10000000,
				(int)TRUE, systemsid, (int)0xb, (u32)0x10000000,
				(int)TRUE, adminsid, (int)0xb, (u32)0x10000000);
			expectacc = expect = 0700;
			expectdef = 0700;
			break;
		case 8 :  /* WinXP/JP2 */
			isdir = TRUE;
			descr = build_dummy_descr(isdir, adminsid, systemsid,
				6,
				(int)TRUE, owner1,    (int)0x0, (u32)0x1f01ff,
				(int)TRUE, systemsid, (int)0x0, (u32)0x1f01ff,
				(int)TRUE, adminsid,  (int)0x0, (u32)0x1f01ff,
				(int)TRUE, owner1,    (int)0xb, (u32)0x10000000,
				(int)TRUE, systemsid, (int)0xb, (u32)0x10000000,
				(int)TRUE, adminsid,  (int)0xb, (u32)0x10000000);
			expectacc = expect = 0700;
			expectdef = 0700;
			break;
		default :
			expectacc = expectdef = 0;
			break;
		}
		if (descr) {
			perms = linux_permissions(descr, isdir);
			if (perms != expect) {
				printf("** Error in sample %d, perms 0%03o expected 0%03o\n",
					cnt,perms,expect);
				showall(descr,0);
				errors++;
			} else {
#if POSIXACLS
				pxdesc = linux_permissions_posix(descr, isdir);
				if (pxdesc) {
					accrights = merge_rights(pxdesc,FALSE);
					defrights = merge_rights(pxdesc,TRUE);
					if (!(pxdesc->tagsset & ~(POSIX_ACL_USER_OBJ | POSIX_ACL_GROUP_OBJ | POSIX_ACL_OTHER)))
						mixmode = expect;
					else
						mixmode = (expect & 07707) | ((accrights >> 9) & 070);
					if ((pxdesc->mode != mixmode)
					  || (accrights != expectacc)
					  || (defrights != expectdef)) {
						printf("** Error in sample %d : mode %03o expected 0%03o\n",
							cnt,pxdesc->mode,mixmode);
						printf("     Posix access rights 0%03lo expected 0%03lo\n",
							(long)accrights,(long)expectacc);
						printf("          default rights 0%03lo expected 0%03lo\n",
							(long)defrights,(long)expectdef);
						showall(descr,0);
						showposix(pxdesc);
exit(1);
					}
					free(pxdesc);
				}
#endif
			}
		free(descr);
		}
	}
}


/*
 *		Check whether any basic permission setting is interpreted
 *	back exactly as set
 */

void basictest(int kind, BOOL isdir, const SID *owner, const SID *group)
{
	char *attr;
	mode_t perm;
	mode_t gotback;
	u32 count;
	u32 acecount;
	u32 globhash;
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	enum { ERRNO,
		ERRMA, ERRPA, /* error converting mode or Posix ACL to NTFS */
		ERRAM, ERRAP, /* error converting NTFS to mode or Posix ACL */
	} err;
	u32 expectcnt[] = {
		27800, 31896,
		24064, 28160,
		24064, 28160,
		24064, 28160,
		25416, 29512
	} ;
	u32 expecthash[] = {
		0x8f80865b, 0x7bc7960,
		0x8fd9ecfe, 0xddd4db0,
		0xa8b07400, 0xa189c20,
		0xc5689a00, 0xb6c09000,
		0x94bfb419, 0xa4311791
	} ;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
	char *pxattr;
	u32 pxcount;
	u32 pxacecount;
	u32 pxglobhash;
#endif

	count = 0;
	acecount = 0;
	globhash = 0;
#if POSIXACLS
	pxcount = 0;
	pxacecount = 0;
	pxglobhash = 0;
#endif
	for (perm=0; (perm<=07777) && (errors < 10); perm++) {
		err = ERRNO;
		/* file owned by plain user and group */
		attr = ntfs_build_descr(perm,isdir,owner,(const SID*)group);
		if (attr && ntfs_valid_descr(attr, ntfs_attr_size(attr))) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
			pacl = (const ACL*)&attr[le32_to_cpu(phead->dacl)];
			acecount += le16_to_cpu(pacl->ace_count);
			globhash += hash((const le32*)attr,ntfs_attr_size(attr));
			count++;
#if POSIXACLS
			/*
			 * Build a NTFS ACL from a mode, and
			 * decode to a Posix ACL, expecting to
			 * get the original mode back.
			 */
			pxdesc = linux_permissions_posix(attr, isdir);
			if (!pxdesc || (pxdesc->mode != perm)) {
				err = ERRAP;
				if (pxdesc)
					gotback = pxdesc->mode;
				else
					gotback = 0;
			} else {
			/*
			 * Build a NTFS ACL from the Posix ACL, expecting to
			 * get exactly the same NTFS ACL, then decode to a
			 * mode, expecting to get the original mode back.
			 */
				pxattr = ntfs_build_descr_posix(context.mapping,
						pxdesc,isdir,owner,
						(const SID*)group);
				if (pxattr && !memcmp(pxattr,attr,
						 ntfs_attr_size(attr))) {
					phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
					pacl = (const ACL*)&attr[le32_to_cpu(phead->dacl)];
					pxacecount += le16_to_cpu(pacl->ace_count);
					pxglobhash += hash((const le32*)attr,ntfs_attr_size(attr));
					pxcount++;
					gotback = linux_permissions(pxattr, isdir);
					if (gotback != perm)
						err = ERRAM;
					else
						free(pxattr);
				} else
					err = ERRPA;
				free(attr);
			}
			free(pxdesc);
#else
			gotback = linux_permissions(attr, isdir);
			if (gotback != perm)
				err = ERRAM;
			else
				free(attr);
#endif /* POSIXACLS */
		} else
			err = ERRMA;

		switch (err) {
		case ERRMA :
			printf("** no or wrong permission settings "
				"for kind %d perm %03o\n",kind,perm);
			if (attr && opt_v)
				hexdump(attr,ntfs_attr_size(attr),8);
			if (attr && (opt_v >= 2)) {
				showheader(attr,4);
				showusid(attr,4);
				showgsid(attr,4);
				showdacl(attr,isdir,4);
				showsacl(attr,isdir,4);
			}
			errors++;
			break;
		case ERRPA :
			printf("** no or wrong permission settings from PX "
				"for kind %d perm %03o\n",kind,perm);
			errors++;
			break;
#if POSIXACLS
		case ERRAM :
			printf("** wrong permission settings, "
				"kind %d perm 0%03o, gotback %03o\n",
				kind, perm, gotback);
			if (opt_v)
				hexdump(pxattr,ntfs_attr_size(pxattr),8);
			if (opt_v >= 2) {
				showheader(pxattr,4);
				showusid(pxattr,4);
				showgsid(pxattr,4);
				showdacl(pxattr,isdir,4);
				showsacl(pxattr,isdir,4);
			}
			errors++;
			break;
		case ERRAP :
			/* continued */
#else
		case ERRAM :
		case ERRAP :
#endif /* POSIXACLS */
			printf("** wrong permission settings, "
				"kind %d perm 0%03o, gotback %03o\n",
				kind, perm, gotback);
			if (opt_v)
				hexdump(attr,ntfs_attr_size(attr),8);
			if (opt_v >= 2) {
				showheader(attr,4);
				showusid(attr,4);
				showgsid(attr,4);
				showdacl(attr,isdir,4);
				showsacl(attr,isdir,4);
			}
			errors++;
			free(attr);
			break;
		default :
			break;
		}
	}
	printf("%lu ACLs built from mode, %lu ACE built, mean count %lu.%02lu\n",
		(unsigned long)count,(unsigned long)acecount,
		(unsigned long)acecount/count,acecount*100L/count%100L);
	if (acecount != expectcnt[kind]) {
		printf("** Error : expected ACE count %lu\n",
			(unsigned long)expectcnt[kind]);
		errors++;
	}
	if (globhash != expecthash[kind]) {
		printf("** Error : wrong global hash 0x%lx instead of 0x%lx\n",
			(unsigned long)globhash, (unsigned long)expecthash[kind]);
		errors++;
	}
#if POSIXACLS
	printf("%lu ACLs built from Posix ACLs, %lu ACE built, mean count %lu.%02lu\n",
		(unsigned long)pxcount,(unsigned long)pxacecount,
		(unsigned long)pxacecount/pxcount,pxacecount*100L/pxcount%100L);
	if (pxacecount != expectcnt[kind]) {
		printf("** Error : expected ACE count %lu\n",
			(unsigned long)expectcnt[kind]);
		errors++;
	}
	if (pxglobhash != expecthash[kind]) {
		printf("** Error : wrong global hash 0x%lx instead of 0x%lx\n",
			(unsigned long)pxglobhash, (unsigned long)expecthash[kind]);
		errors++;
	}
#endif /* POSIXACLS */
}

#if POSIXACLS

/*
 *		Check whether Posix ACL settings are interpreted
 *	back exactly as set
 */

void posixtest(int kind, BOOL isdir,
			const SID *owner, const SID *group)
{
	struct POSIX_SECURITY *pxdesc;
	struct {
		struct POSIX_SECURITY pxdesc;
		struct POSIX_ACE aces[10];
	} desc;
	int ownobj;
	int grpobj;
	int usr;
	int grp;
	int wrld;
	int mask;
	int mindes, maxdes;
	int minmsk, maxmsk;
	char *pxattr;
	u32 count;
	u32 acecount;
	u32 globhash;
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	struct POSIX_SECURITY *gotback;
	enum { ERRNO,
		ERRMA, ERRPA, /* error converting mode or Posix ACL to NTFS */
		ERRAM, ERRAP, /* error converting NTFS to mode or Posix ACL */
	} err;
	u32 expectcnt[] = {
#ifdef STSC
		32400, 34992,
		25920, 28512,
		25920, 28512,
		25920, 28512,
		26460, 29052,
		0, 0,
		0, 0,
		0, 0,
		24516, 27108,
		20736, 23328,
		20736, 23328,
		20736, 23328,
		21060, 23652,
#else
		252720, 273456,
		199584, 220320,
		199584, 220320,
		199584, 220320,
		203904, 224640,
		0, 0,
		0, 0,
		0, 0,
		196452, 217188,
		165888, 186624,
		165888, 186624,
		165888, 186624,
		168480, 189216,
#endif
		0, 0,
		0, 0,
		0, 0,
		16368, 18672,
		0, 0,
		13824, 0,
		0, 0,
		14640, 0
	} ;
	u32 expecthash[] = {
#ifdef STSC
		0xf9f82115, 0x40666647,
		0xde826d30, 0xa181b660,
		0x952d4500, 0x8ac49450,
		0xf80acee0, 0xbd9ec6c0,
		0xfe09b868, 0xde24e84d,
		0, 0,
		0, 0,
		0, 0,
		0x2381438d, 0x3ab42dc6,
		0x7cccf6f8, 0x108ad430,
		0x5e448840, 0x83ab6c40,
		0x9b037100, 0x8f7c3b40,
		0x04a359dc, 0xa4619609,
#else
		0x1808a6cd, 0xd82f7c60,
		0x5ad29e85, 0x518c7620,
		0x188ce270, 0x7e44e590,
		0x48a64800, 0x5bdf0030,
		0x1c64aec6, 0x8b0168fa,
		0, 0,
		0, 0,
		0, 0,
		0x169fb80e, 0x382d9a59,
		0xf9c28164, 0x1855d352,
		0xf9685700, 0x44d16700,
		0x587ebe90, 0xf7c51480,
		0x2cb1b518, 0x52408df6,
#endif
		0, 0,
		0, 0,
		0, 0,
		0x905f2e38, 0xd40c22f0,
		0, 0,
		0xdd76da00, 0,
		0, 0,
		0x718e34a0, 0
	};

	count = 0;
	acecount = 0;
	globhash = 0;
				/* fill headers */
	pxdesc = &desc.pxdesc;
	pxdesc->mode = 0;
	pxdesc->defcnt = 0;
	if (kind & 32) {
		pxdesc->acccnt = 4;
		pxdesc->firstdef = 4;
		pxdesc->tagsset = 0x35;
	} else {
		pxdesc->acccnt = 6;;
		pxdesc->firstdef = 6;
		pxdesc->tagsset = 0x3f;
	}
	pxdesc->acl.version = POSIX_VERSION;
	pxdesc->acl.flags = 0;
	pxdesc->acl.filler = 0;
				/* prefill aces */
	pxdesc->acl.ace[0].tag = POSIX_ACL_USER_OBJ;
	pxdesc->acl.ace[0].id = -1;
	if (kind & 32) {
		pxdesc->acl.ace[1].tag = POSIX_ACL_GROUP_OBJ;
		pxdesc->acl.ace[1].id = -1;
		pxdesc->acl.ace[2].tag = POSIX_ACL_MASK;
		pxdesc->acl.ace[2].id = -1;
		pxdesc->acl.ace[3].tag = POSIX_ACL_OTHER;
		pxdesc->acl.ace[3].id = -1;
	} else {
		pxdesc->acl.ace[1].tag = POSIX_ACL_USER;
		pxdesc->acl.ace[1].id = (kind & 16 ? 0 : 1000);
		pxdesc->acl.ace[2].tag = POSIX_ACL_GROUP_OBJ;
		pxdesc->acl.ace[2].id = -1;
		pxdesc->acl.ace[3].tag = POSIX_ACL_GROUP;
		pxdesc->acl.ace[3].id = (kind & 16 ? 0 : 1002);
		pxdesc->acl.ace[4].tag = POSIX_ACL_MASK;
		pxdesc->acl.ace[4].id = -1;
		pxdesc->acl.ace[5].tag = POSIX_ACL_OTHER;
		pxdesc->acl.ace[5].id = -1;
	}

	mindes = 3;
	maxdes = (kind & 32 ? mindes : 6);
#ifdef STSC
	minmsk = (kind & 32 ? 0 : 3);
	maxmsk = (kind & 32 ? 7 : 3);
#else
	minmsk = 0;
	maxmsk = 7;
#endif
	for (mask=minmsk; mask<=maxmsk; mask++)
	for (ownobj=1; ownobj<7; ownobj++)
	for (grpobj=1; grpobj<7; grpobj++)
	for (wrld=0; wrld<8; wrld++)
	for (usr=mindes; usr<=maxdes; usr++)
	if (usr != 4)
	for (grp=mindes; grp<=maxdes; grp++)
	if (grp != 4) {
		pxdesc->mode = (ownobj << 6) | (mask << 3) | wrld;

		pxdesc->acl.ace[0].perms = ownobj;
		if (kind & 32) {
			pxdesc->acl.ace[1].perms = grpobj;
			pxdesc->acl.ace[2].perms = mask;
			pxdesc->acl.ace[3].perms = wrld;
		} else {
			pxdesc->acl.ace[1].perms = usr;
			pxdesc->acl.ace[2].perms = grpobj;
			pxdesc->acl.ace[3].perms = grp;
			pxdesc->acl.ace[4].perms = mask;
			pxdesc->acl.ace[5].perms = wrld;
		}

		err = ERRNO;
		gotback = (struct POSIX_SECURITY*)NULL;
		pxattr = ntfs_build_descr_posix(context.mapping,
				pxdesc,isdir,owner,group);
		if (pxattr && ntfs_valid_descr(pxattr, ntfs_attr_size(pxattr))) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)pxattr;
			pacl = (const ACL*)&pxattr[le32_to_cpu(phead->dacl)];
			acecount += le16_to_cpu(pacl->ace_count);
			globhash += hash((const le32*)pxattr,ntfs_attr_size(pxattr));
			count++;
			gotback = linux_permissions_posix(pxattr, isdir);
			if (gotback) {
				if (ntfs_valid_posix(gotback)) {
					if (!same_posix(pxdesc,gotback)) {
						printf("Non matching got back Posix ACL\n");
						printf("input ACL\n");
						showposix(pxdesc);
						printf("NTFS owner\n");
						showusid(pxattr,4);
						printf("NTFS group\n");
						showgsid(pxattr,4);
						printf("NTFS DACL\n");
						showdacl(pxattr,isdir,4);
						printf("gotback ACL\n");
						showposix(gotback);
						errors++;
exit(1);
					}
				} else {
					printf("Got back an invalid Posix ACL\n");
					errors++;
				}
				free(gotback);
			} else {
				printf("Could not get Posix ACL back\n");
				errors++;
			}

		} else {
			printf("NTFS ACL incorrect or not build\n");
			printf("input ACL\n");
			showposix(pxdesc);
			printf("NTFS DACL\n");
			if (pxattr)
				showdacl(pxattr,isdir,4);
			else
				printf("   (none)\n");
			if (gotback) {
				printf("gotback ACL\n");
				showposix(gotback);
			} else
				printf("no gotback ACL\n");
			errors++;
		}
		if (pxattr)
			free(pxattr);
	}
	printf("%lu ACLs built from Posix ACLs, %lu ACE built, mean count %lu.%02lu\n",
		(unsigned long)count,(unsigned long)acecount,
		(unsigned long)acecount/count,acecount*100L/count%100L);
	if (acecount != expectcnt[kind]) {
		printf("** Error ! expected ACE count %lu\n",
			(unsigned long)expectcnt[kind]);
		errors++;
	}
	if (globhash != expecthash[kind]) {
		printf("** Error : wrong global hash 0x%lx instead of 0x%lx\n",
			(unsigned long)globhash, (unsigned long)expecthash[kind]);
		errors++;
	}
}

#endif /* POSIXACLS */

void selftests(void)
{
	le32 owner_sid[] = /* S-1-5-21-3141592653-589793238-462843383-1016 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(1016)
		} ;
	le32 group_sid[] = /* S-1-5-21-3141592653-589793238-462843383-513 */
		{
		cpu_to_le32(0x501), cpu_to_le32(0x05000000), cpu_to_le32(21),
		cpu_to_le32(DEFSECAUTH1), cpu_to_le32(DEFSECAUTH2),
		cpu_to_le32(DEFSECAUTH3), cpu_to_le32(513)
		} ;
#if POSIXACLS
#ifdef STSC
	unsigned char kindlist[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
			   16, 17, 18, 20, 22, 24,
			   32, 33, 36, 40 } ;
#else
	unsigned char kindlist[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
			   16, 17, 18, 20, 22, 24, 19, 21, 23, 25,
			   32, 33, 36, 40 } ;
#endif
	unsigned int k;
#endif /* POSIXACLS */
	int kind;
	const SID *owner;
	const SID *group;
	BOOL isdir;

#if POSIXACLS
	local_build_mapping(context.mapping, (const char*)NULL);
#endif
			/* first check samples */
	mappingtype = MAPDUMMY;
	check_samples();
if (errors) exit(1);
		/*
		 * kind is oring of :
		 *   1 : directory
		 *   2 : owner is root
		 *   4 : group is root
		 *   8 : group is owner
		 *  16 : root is designated user/group
		 *  32 : mask present with no designated user/group
		 */
	for (kind=0; (kind<10) && (errors<10); kind++) {
		isdir = kind & 1;
		if (kind & 8)
			owner = (const SID*)group_sid;
		else
			owner = (kind & 2 ? adminsid : (const SID*)owner_sid);
		group = (kind & 4 ? adminsid : (const SID*)group_sid);
		basictest(kind, isdir, owner, group);
	}
#if POSIXACLS
	for (k=0; (k<sizeof(kindlist)) && (errors<10); k++) {
		kind = kindlist[k];
		isdir = kind & 1;
		if (kind & 8)
			owner = (const SID*)group_sid;
		else
			owner = (kind & 2 ? adminsid : (const SID*)owner_sid);
		group = (kind & 4 ? adminsid : (const SID*)group_sid);
		posixtest(kind, isdir, owner, group);
	}
	ntfs_free_mapping(context.mapping);
#endif
	if (errors >= 10)
		printf("** too many errors, test aborted\n");
}
#endif /* SELFTESTS & !USESTUBS */

#ifdef WIN32

/*
 *		   Get the security descriptor of a file (Windows version)
 */

unsigned int getfull(char *attr, const char *fullname)
{
	static char part[MAXATTRSZ];
	BIGSID ownsid;
	int xowner;
	int ownersz;
	u16 ownerfl;
	ULONG attrsz;
	ULONG partsz;
	BOOL overflow;

	attrsz = 0;
	partsz = 0;
	overflow = FALSE;
	if (GetFileSecurityW((LPCWSTR)fullname,OWNER_SECURITY_INFORMATION,
				(char*)part,MAXATTRSZ,&partsz)) {
		xowner = get4l(part,4);
		if (xowner) {
			ownerfl = get2l(part,2);
			ownersz = ntfs_sid_size((SID*)&part[xowner]);
			if (ownersz <= (int)sizeof(BIGSID))
				memcpy(ownsid,&part[xowner],ownersz);
			else
				overflow = TRUE;
		} else {
			ownerfl = 0;
			ownersz = 0;
		}
			/*
			 *  SACL : just feed in or clean
			 */
		if (!GetFileSecurityW((LPCWSTR)fullname,SACL_SECURITY_INFORMATION,
				(char*)attr,MAXATTRSZ,&attrsz)) {
			attrsz = 20;
			set4l(attr,0);
			attr[0] = SECURITY_DESCRIPTOR_REVISION;
			set4l(&attr[12],0);
			if (opt_v >= 2)
				printf("   No SACL\n");
		}
			/*
			 *  append DACL and merge its flags
			 */
		partsz = 0;
		set4l(&attr[16],0);
		if (GetFileSecurityW((LPCWSTR)fullname,DACL_SECURITY_INFORMATION,
			    (char*)part,MAXATTRSZ,&partsz)) {
			if ((attrsz + partsz - 20) <= MAXATTRSZ) {
				memcpy(&attr[attrsz],&part[20],partsz-20);
				set4l(&attr[16],(partsz > 20 ? attrsz : 0));
				set2l(&attr[2],get2l(attr,2) | (get2l(part,2)
					& const_le16_to_cpu(SE_DACL_PROTECTED
						   | SE_DACL_AUTO_INHERITED
						   | SE_DACL_PRESENT)));
				attrsz += partsz - 20;
			} else
				overflow = TRUE;
		} else
			if (partsz > MAXATTRSZ)
				overflow = TRUE;
			else {
				if (opt_b)
					printf("#   No discretionary access control list\n");
				else
					printf("   No discretionary access control list\n");
				warnings++;
			}

			/*
			 *  append owner and merge its flag
			 */
		if (xowner && !overflow) {
			memcpy(&attr[attrsz],ownsid,ownersz);
			set4l(&attr[4],attrsz);
			set2l(&attr[2],get2l(attr,2)
			   | (ownerfl & const_le16_to_cpu(SE_OWNER_DEFAULTED)));
			attrsz += ownersz;
		} else
			set4l(&attr[4],0);
			/*
			 * append group
			 */
		partsz = 0;
		set4l(&attr[8],0);
		if (GetFileSecurityW((LPCWSTR)fullname,GROUP_SECURITY_INFORMATION,
			    (char*)part,MAXATTRSZ,&partsz)) {
			if ((attrsz + partsz - 20) <= MAXATTRSZ) {
				memcpy(&attr[attrsz],&part[20],partsz-20);
				set4l(&attr[8],(partsz > 20 ? attrsz : 0));
				set2l(&attr[2],get2l(attr,2) | (get2l(part,2)
					& const_le16_to_cpu(SE_GROUP_DEFAULTED)));
				attrsz += partsz - 20;
			} else
				overflow = TRUE;
		} else
			if (partsz > MAXATTRSZ)
				overflow = TRUE;
			else {
				printf("**   No group SID\n");
				warnings++;
			}
		set2l(&attr[2],get2l(attr,2)
					| const_le16_to_cpu(SE_SELF_RELATIVE));
		if (overflow) {
			printf("** Descriptor was too long (> %d)\n",MAXATTRSZ);
			warnings++;
			attrsz = 0;
		} else
			if (!ntfs_valid_descr((char*)attr,attrsz)) {
				printf("** Descriptor for ");
				printname(stdout,fullname);
				printf(" is not valid\n");
				errors++;
				attrsz = 0;
			}

	} else {
		printf("** Could not get owner of ");
		printname(stdout,fullname);
		printf(", partsz %d\n",partsz);
		printerror(stdout);
		warnings++;
		attrsz = 0;		
	}
	return (attrsz);
}

/*
 *		Update a security descriptor (Windows version)
 */

BOOL updatefull(const char *name, DWORD flags, char *attr)
{
	BOOL bad;

	bad = !SetFileSecurityW((LPCWSTR)name, flags, attr);
	if (bad
	  && (flags & OWNER_SECURITY_INFORMATION)
	  && (GetLastError() == 1307)) {
		printf("** Could not set owner of ");
		printname(stdout,name);
		printf(", retrying with no owner setting\n");
		warnings++;
		bad = !SetFileSecurityW((LPCWSTR)name,
			flags & ~OWNER_SECURITY_INFORMATION, (char*)attr);
	}
	if (bad) {
		printf("** Could not change attributes of ");
		printname(stdout,name);
		printf("\n");
		printerror(stdout);
		errors++;
	}
	return (!bad);
}

#else

/*
 *		   Get the security descriptor of a file (Linux version)
 */

unsigned int getfull(char *attr, const char *fullname)
{
	static char part[MAXATTRSZ];
	BIGSID ownsid;
	int xowner;
	int ownersz;
	u16 ownerfl;
	u32 attrsz;
	u32 partsz;
	BOOL overflow;

	attrsz = 0;
	partsz = 0;
	overflow = FALSE;
	if (ntfs_get_file_security(ntfs_context,fullname,
				OWNER_SECURITY_INFORMATION,
				(char*)part,MAXATTRSZ,&partsz)) {
		xowner = get4l(part,4);
		if (xowner) {
			ownerfl = get2l(part,2);
			ownersz = ntfs_sid_size((SID*)&part[xowner]);
			if (ownersz <= (int)sizeof(BIGSID))
				memcpy(ownsid,&part[xowner],ownersz);
			else
				overflow = TRUE;
		} else {
			ownerfl = 0;
			ownersz = 0;
		}
			/*
			 *  SACL : just feed in or clean
			 */
		if (!ntfs_get_file_security(ntfs_context,fullname,
				SACL_SECURITY_INFORMATION,
				(char*)attr,MAXATTRSZ,&attrsz)) {
			attrsz = 20;
			set4l(attr,0);
			attr[0] = SECURITY_DESCRIPTOR_REVISION;
			set4l(&attr[12],0);
			if (opt_v >= 2)
				printf("   No SACL\n");
		}
			/*
			 *  append DACL and merge its flags
			 */
		partsz = 0;
		set4l(&attr[16],0);
		if (ntfs_get_file_security(ntfs_context,fullname,
		    DACL_SECURITY_INFORMATION,
		    (char*)part,MAXATTRSZ,&partsz)) {
			if ((attrsz + partsz - 20) <= MAXATTRSZ) {
				memcpy(&attr[attrsz],&part[20],partsz-20);
				set4l(&attr[16],(partsz > 20 ? attrsz : 0));
				set2l(&attr[2],get2l(attr,2) | (get2l(part,2)
					& const_le16_to_cpu(SE_DACL_PROTECTED
						   | SE_DACL_AUTO_INHERITED
						   | SE_DACL_PRESENT)));
				attrsz += partsz - 20;
			} else
				overflow = TRUE;
		} else
			if (partsz > MAXATTRSZ)
				overflow = TRUE;
			else {
				if (opt_b)
					printf("#   No discretionary access control list\n");
				else
					printf("   No discretionary access control list\n");
				warnings++;
			}

			/*
			 *  append owner and merge its flag
			 */
		if (xowner && !overflow) {
			memcpy(&attr[attrsz],ownsid,ownersz);
			set4l(&attr[4],attrsz);
			set2l(&attr[2],get2l(attr,2)
			   | (ownerfl & const_le16_to_cpu(SE_OWNER_DEFAULTED)));
			attrsz += ownersz;
		} else
			set4l(&attr[4],0);
			/*
			 * append group
			 */
		partsz = 0;
		set4l(&attr[8],0);
		if (ntfs_get_file_security(ntfs_context,fullname,
		    GROUP_SECURITY_INFORMATION,
		    (char*)part,MAXATTRSZ,&partsz)) {
			if ((attrsz + partsz - 20) <= MAXATTRSZ) {
				memcpy(&attr[attrsz],&part[20],partsz-20);
				set4l(&attr[8],(partsz > 20 ? attrsz : 0));
				set2l(&attr[2],get2l(attr,2) | (get2l(part,2)
					& const_le16_to_cpu(SE_GROUP_DEFAULTED)));
				attrsz += partsz - 20;
			} else
				overflow = TRUE;
		} else
			if (partsz > MAXATTRSZ)
				overflow = TRUE;
			else {
				printf("**   No group SID\n");
				warnings++;
			}
		if (overflow) {
			printf("** Descriptor was too long (> %d)\n",MAXATTRSZ);
			warnings++;
			attrsz = 0;
		} else
			if (!ntfs_valid_descr((char*)attr,attrsz)) {
				printf("** Descriptor for %s is not valid\n",fullname);
				errors++;
				attrsz = 0;
			}

	} else {
		printf("** Could not get owner of %s\n",fullname);
		warnings++;
		attrsz = 0;		
	}
	return (attrsz);
}

/*
 *		Update a security descriptor (Linux version)
 */

BOOL updatefull(const char *name, DWORD flags, char *attr)
{
	BOOL ok;

	ok = !ntfs_set_file_security(ntfs_context, name, flags, attr);
	if (ok) {
		printf("** Could not change attributes of %s\n",name);
		printerror(stdout);
		errors++;
	}
	return (ok);
}


#endif

#if POSIXACLS

/*
 *		   Set all the parameters associated to a file
 */

BOOL setfull_posix(const char *fullname, const struct POSIX_SECURITY *pxdesc,
			BOOL isdir)
{
	static char attr[MAXATTRSZ];
	struct POSIX_SECURITY *oldpxdesc;
	struct POSIX_SECURITY *newpxdesc;
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	char *newattr;
	int err;
	unsigned int attrsz;
	int newattrsz;
	const SID *usid;
	const SID *gsid;
#if OWNERFROMACL
	const SID *osid;
#endif

	printf("%s ",(isdir ? "Directory" : "File"));
	printname(stdout,fullname);
	if (pxdesc->acccnt)
		printf("\n");
	else
		printf(" mode 0%03o\n",pxdesc->mode);

	err = FALSE;
	attrsz = getfull(attr, fullname);
	if (attrsz) {
		oldpxdesc = linux_permissions_posix(attr, isdir);
		if (opt_v >= 2) {
			printf("Posix equivalent of old ACL :\n");
			showposix(oldpxdesc);
		}
		if (oldpxdesc) {
			if (!pxdesc->defcnt
			   && !(pxdesc->tagsset &
			     (POSIX_ACL_USER | POSIX_ACL_GROUP | POSIX_ACL_MASK))) {
				if (!ntfs_merge_mode_posix(oldpxdesc,pxdesc->mode))
					newpxdesc = oldpxdesc;
				else {
					newpxdesc = (struct POSIX_SECURITY*)NULL;
					free(oldpxdesc);
				}
			} else {
				newpxdesc = ntfs_merge_descr_posix(pxdesc, oldpxdesc);
				free(oldpxdesc);
			}
			if (opt_v) {
				printf("New Posix ACL :\n");
				showposix(newpxdesc);
			}
		} else
			newpxdesc = (struct POSIX_SECURITY*)NULL;
		if (newpxdesc) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
			gsid = (const SID*)&attr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
			osid = (const SID*)&attr[le32_to_cpu(phead->owner)];
			usid = ntfs_acl_owner((const char*)attr);
			if (!ntfs_same_sid(usid,osid))
				printf("== Windows owner might change\n"); 
#else
			usid = (const SID*)&attr[le32_to_cpu(phead->owner)];
#endif
			newattr = ntfs_build_descr_posix(context.mapping,
				newpxdesc,isdir,usid,gsid);
			free(newpxdesc);
		} else
			newattr = (char*)NULL;
		if (newattr) {
			newattrsz = ntfs_attr_size(newattr);
			if (opt_v) {
				printf("New NTFS security descriptor\n");
				hexdump(newattr,newattrsz,4);
			}
			if (opt_v >= 2) {
				printf("Expected hash : 0x%08lx\n",
					(unsigned long)hash((le32*)newattr,ntfs_attr_size(newattr)));
				showheader(newattr,0);
				showusid(newattr,0);
				showgsid(newattr,0);
				showdacl(newattr,isdir,0);
				showsacl(newattr,isdir,0);
			}

#ifdef WIN32
			/*
			 * avoid getting a set owner error on Windows
			 * owner should not be changed anyway
			 */
			if (!updatefull(fullname,
				DACL_SECURITY_INFORMATION
				| GROUP_SECURITY_INFORMATION
				| OWNER_SECURITY_INFORMATION,
					newattr))
#else
			if (!updatefull(fullname,
				DACL_SECURITY_INFORMATION
				| GROUP_SECURITY_INFORMATION
				| OWNER_SECURITY_INFORMATION,
					newattr))
#endif
				err = TRUE;
/*
{
struct POSIX_SECURITY *interp;
printf("Reinterpreted new Posix :\n");
interp = linux_permissions_posix(newattr,isdir);
showposix(interp);
free(interp);
}
*/
			free(newattr);
		} else
			err = TRUE;
	} else
		err = TRUE;
	return (!err);
}

#endif

BOOL setfull(const char *fullname, int mode, BOOL isdir)
{
	static char attr[MAXATTRSZ];
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	char *newattr;
	int err;
	unsigned int attrsz;
	int newattrsz;
	const SID *usid;
	const SID *gsid;
#if OWNERFROMACL
	const SID *osid;
#endif

	printf("%s ",(isdir ? "Directory" : "File"));
	printname(stdout,fullname);
	printf(" mode 0%03o\n",mode);
	attrsz = getfull(attr, fullname);
	err = FALSE;
	if (attrsz) {
		phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
		gsid = (const SID*)&attr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
		osid = (const SID*)&attr[le32_to_cpu(phead->owner)];
		usid = ntfs_acl_owner((const char*)attr);
		if (!ntfs_same_sid(usid,osid))
			printf("== Windows owner might change\n"); 
#else
		usid = (const SID*)&attr[le32_to_cpu(phead->owner)];
#endif
		newattr = ntfs_build_descr(mode,isdir,usid,gsid);
		if (newattr) {
			newattrsz = ntfs_attr_size(newattr);
			if (opt_v) {
				printf("Security descriptor\n");
				hexdump(newattr,newattrsz,4);
			}
			if (opt_v >= 2) {
				printf("Expected hash : 0x%08lx\n",
					(unsigned long)hash((le32*)newattr,ntfs_attr_size(newattr)));
				showheader(newattr,0);
				showusid(newattr,0);
				showgsid(newattr,0);
				showdacl(newattr,isdir,0);
				showsacl(newattr,isdir,0);
			}

#ifdef WIN32
			/*
			 * avoid getting a set owner error on Windows
			 * owner should not be changed anyway
			 */
			if (!updatefull(fullname,
				DACL_SECURITY_INFORMATION
				| GROUP_SECURITY_INFORMATION
				| OWNER_SECURITY_INFORMATION,
					newattr))
#else
			if (!updatefull(fullname,
				DACL_SECURITY_INFORMATION
				| GROUP_SECURITY_INFORMATION
				| OWNER_SECURITY_INFORMATION,
					newattr))
#endif
				err = TRUE;
			free(newattr);
		}
		
	} else
		err = TRUE;
	return (err);
}

#ifdef WIN32

/*
 *		Check whether a directory with reparse data is a junction
 *	or a symbolic link
 */

BOOL islink(const char *filename)
{
	WIN32_FIND_DATAW found;
	HANDLE search;
	BOOL link;

	link = FALSE;
	search = FindFirstFileW((LPCWSTR)filename, &found);
	if (search != INVALID_HANDLE_VALUE) {
		link = (found.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT)
		   || (found.dwReserved0 == IO_REPARSE_TAG_SYMLINK);
		FindClose(search);
	}
	return (link);
}

#if POSIXACLS
BOOL iterate(RECURSE call, const char *fullname, const struct POSIX_SECURITY *pxdesc)
#else
BOOL iterate(RECURSE call, const char *fullname, mode_t mode)
#endif
{
	WIN32_FIND_DATAW found;
	HANDLE search;
	BOOL err;
	unsigned int len;
	char *filter;
	char *inner;

	err = FALSE;
	filter = (char*)malloc(MAXFILENAME);
	inner = (char*)malloc(MAXFILENAME);
	if (filter && inner) {
		len = utf16len(fullname);
		memcpy(filter, fullname, 2*len);
		makeutf16(&filter[2*len],"\\*.*");
		search = FindFirstFileW((LPCWSTR)filter, &found);
		if (search != INVALID_HANDLE_VALUE) {
			do {
				if (found.cFileName[0] != UNICODE('.')) {
					memcpy(inner, fullname, 2*len);
					inner[2*len] = '\\';
					inner[2*len+1] = 0;
					memcpy(&inner[2*len+2],found.cFileName,
						2*utf16len((char*)found.cFileName)+2);
					if (opt_v)
						if (opt_b)
							printf("#\n#\n");
						else
							printf("\n\n");
					switch (call) {
					case RECSHOW :
						if (recurseshow(inner))
							err = TRUE;
						break;
#if POSIXACLS
					case RECSETPOSIX :
						if (recurseset_posix(inner,pxdesc))
							err = TRUE;
						break;
#else
					case RECSET :
						if (recurseset(inner,mode))
							err = TRUE;
						break;
#endif
					default :
						err = TRUE;
					}
				}
			} while (FindNextFileW(search, &found));
			FindClose(search);
		}
		free(filter);
		free(inner);
	} else {
		printf("** Cannot process deeper : not enough memory\n");
		errors++;
		err = TRUE;
	}
	return (err);
}



/*
 *		   Display all the parameters associated to a file (Windows version)
 */

void showfull(const char *fullname, BOOL isdir)
{
	static char attr[MAXATTRSZ];
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif
	ULONG attrsz;
	int mode;
	uid_t uid;
	gid_t gid;
	int attrib;
	int level;

	printf("%s ",(isdir ? "Directory" : "File"));
	printname(stdout,fullname);
	printf("\n");

       /* get individual parameters, as when trying to get them */
       /* all, and one (typically SACL) is missing, we get none, */
       /* and concatenate them, to be able to compute the hash code */

	attrsz = getfull(attr, fullname);
	if (attrsz) {
			if (opt_v || opt_b) {
				hexdump(attr,attrsz,8);
				printf("Computed hash : 0x%08lx\n",
					(unsigned long)hash((le32*)attr,attrsz));
			}
			if (opt_v && opt_b) {
				printf("# %s ",(isdir ? "Directory" : "File"));
				printname(stdout,fullname);
				printf(" hash 0x%lx\n",
					(unsigned long)hash((le32*)attr,attrsz));
			}
			attrib = GetFileAttributesW((LPCWSTR)fullname);
			if (attrib == INVALID_FILE_ATTRIBUTES) {
				printf("** Could not get file attrib\n");
				errors++;
			} else
				printf("Windows attrib : 0x%x\n",attrib);
			if (ntfs_valid_descr(attr,attrsz)) {
#if POSIXACLS
				pxdesc = linux_permissions_posix(attr,isdir);
				if (pxdesc)
					mode = pxdesc->mode;
				else
					mode = 0;
#else
				mode = linux_permissions(attr,isdir);
#endif
				if (opt_v >= 2) {
					level = (opt_b ? 4 : 0);
					showheader(attr,level);
					showusid(attr,level);
					showgsid(attr,level);
					showdacl(attr,isdir,level);
					showsacl(attr,isdir,level);
				}
				uid = linux_owner(attr);
				gid = linux_group(attr);
				if (opt_b) {
					printf("# Interpreted Unix owner %d, group %d, mode 0%03o\n",
						(int)uid,(int)gid,mode);
				} else {
					printf("Interpreted Unix owner %d, group %d, mode 0%03o\n",
						(int)uid,(int)gid,mode);
				}
#if POSIXACLS
				if (pxdesc) {
					if (!opt_b
					    && (pxdesc->defcnt
						|| (pxdesc->tagsset
						    & (POSIX_ACL_USER
							| POSIX_ACL_GROUP
							| POSIX_ACL_MASK))))
						showposix(pxdesc);
					free(pxdesc);
				}
#endif
			} else
				if (opt_b)
					printf("# Descriptor fails sanity check\n");
				else
					printf("Descriptor fails sanity check\n");
	}
}

BOOL recurseshow(const char *fullname)
{
	int attrib;
	int err;
	BOOL isdir;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)fullname);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
		showfull(fullname,isdir);
		if (isdir
		   && !((attrib & FILE_ATTRIBUTE_REPARSE_POINT)
			&& islink(fullname))) {
#if POSIXACLS
			err = iterate(RECSHOW, fullname, (struct POSIX_SECURITY*)NULL);
#else
			err = iterate(RECSHOW, fullname, 0);
#endif
		}
	} else {
		printf("** Could not access ");
		printname(stdout,fullname);
		printf("\n");
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (err);
}


BOOL singleshow(const char *fullname)
{
	int attrib;
	int err;
	BOOL isdir;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)fullname);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
		showfull(fullname,isdir);
	} else { 
		printf("** Could not access ");
		printname(stdout,fullname);
		printf("\n");
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (err);
}

#if POSIXACLS

BOOL recurseset_posix(const char *fullname, const struct POSIX_SECURITY *pxdesc)
{
	int attrib;
	int err;
	BOOL isdir;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)fullname);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
		err = !setfull_posix(fullname,pxdesc,isdir);
		if (err) {
			printf("** Failed to update ");
			printname(stdout,fullname);
			printf("\n");
			errors++;
		} else
			if (isdir
			   && !((attrib & FILE_ATTRIBUTE_REPARSE_POINT)
				&& islink(fullname)))
				iterate(RECSETPOSIX, fullname, pxdesc);
	} else { 
		err = GetLastError();
		printf("** Could not access ");
		printname(stdout,fullname);
		printf("\n");
		printerror(stdout);
		err = TRUE;
		errors++;
	}
	return (err);
}

#else

BOOL recurseset(const char *fullname, int mode)
{
	int attrib;
	int err;
	BOOL isdir;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)fullname);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
		setfull(fullname,mode,isdir);
		if (isdir
		   && !((attrib & FILE_ATTRIBUTE_REPARSE_POINT)
			&& islink(fullname)))
			iterate(RECSETPOSIX, fullname, mode);
	} else { 
		err = GetLastError();
		printf("** Could not access ");
		printname(stdout,fullname);
		printf("\n");
		printerror(stdout);
		err = TRUE;
		errors++;
	}
	return (err);
}

#endif

#if POSIXACLS

BOOL singleset_posix(const char *path, const struct POSIX_SECURITY *pxdesc)
{
	BOOL isdir;
	BOOL err;
	int attrib;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)path);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY);
		err = !setfull_posix(path,pxdesc,isdir);
		if (err) {
			printf("** Failed to update ");
			printname(stdout,path);
			printf("\n");
			errors++;
		}
	} else {
		printf("** Could not access ");
		printname(stdout,path);
		printf("\n");
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (!err);
}

#endif

BOOL singleset(const char *path, int mode)
{
	BOOL isdir;
	BOOL err;
	int attrib;

	err = FALSE;
	attrib = GetFileAttributesW((LPCWSTR)path);
	if (attrib != INVALID_FILE_ATTRIBUTES) {
		isdir = (attrib & FILE_ATTRIBUTE_DIRECTORY);
		setfull(path,mode,isdir);
	} else {
		printf("** Could not access ");
		printname(stdout,path);
		printf("\n");
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (!err);
}

#else

/*
 *		   Display all the parameters associated to a file (Linux version)
 */

void showfull(const char *fullname, BOOL isdir)
{
	static char attr[MAXATTRSZ];
	static char part[MAXATTRSZ];
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif
	struct SECURITY_DATA *psecurdata;
	char *newattr;
	int securindex;
	int mode;
	int level;
	int attrib;
	u32 attrsz;
	u32 partsz;
	uid_t uid;
	gid_t gid;

	if (opt_v || opt_b)
		printf("%s %s\n",(isdir ? "Directory" : "File"),fullname);

       /* get individual parameters, as when trying to get them */
       /* all, and one (typically SACL) is missing, we get none */
       /* and concatenate them, to be able to compute the checksum */

	partsz = 0;
	securindex = ntfs_get_file_security(ntfs_context,fullname,
				OWNER_SECURITY_INFORMATION,
				(char*)part,MAXATTRSZ,&partsz);

	attrib = ntfs_get_file_attributes(ntfs_context, fullname);
	if (attrib == INVALID_FILE_ATTRIBUTES) {
		printf("** Could not get file attrib\n");
		errors++;
	}
	if ((securindex < 0)
	    || (securindex >= MAXSECURID)
	    || ((securindex > 0)
		&& ((!opt_r && !opt_b)
		   || !securdata[securindex >> SECBLKSZ]
		   || !securdata[securindex >> SECBLKSZ][securindex & ((1 << SECBLKSZ) - 1)].filecount)))
		{
		if (opt_v || opt_b) {
			if ((securindex < -1) || (securindex >= MAXSECURID))
				printf("Security key : 0x%x out of range\n",securindex);
			else
				if (securindex == -1)
					printf("Security key : none\n");
				else
					printf("Security key : 0x%x\n",securindex);
		} else {
			printf("%s %s",(isdir ? "Directory" : "File"),fullname);
			if ((securindex < -1) || (securindex >= MAXSECURID))
					printf(" : key 0x%x out of range\n",securindex);
			else
				if (securindex == -1)
					printf(" : no key\n");
				else
					printf(" : key 0x%x\n",securindex);
		}

		attrsz = getfull(attr, fullname);
		if (attrsz) {
			psecurdata = (struct SECURITY_DATA*)NULL;
			if ((securindex < MAXSECURID) && (securindex > 0)) {
				if (!securdata[securindex >> SECBLKSZ])
					newblock(securindex);
				if (securdata[securindex >> SECBLKSZ])
					psecurdata = &securdata[securindex >> SECBLKSZ]
					   [securindex & ((1 << SECBLKSZ) - 1)];
			}
			if (opt_v && (opt_a || opt_b) && psecurdata) {
				newattr = (char*)malloc(attrsz);
				printf("# %s %s hash 0x%lx\n",(isdir ? "Directory" : "File"),
					fullname,
					(unsigned long)hash((le32*)attr,attrsz));
				if (newattr) {
					memcpy(newattr,attr,attrsz);
					psecurdata->attr = newattr;
				}
			}
			if ((opt_v || opt_b)
				&& ((securindex >= MAXSECURID)
				   || (securindex <= 0)
				   || !psecurdata
				   || (!psecurdata->filecount
					&& !psecurdata->flags))) {
				hexdump(attr,attrsz,8);
				printf("Computed hash : 0x%08lx\n",
					(unsigned long)hash((le32*)attr,attrsz));
			}
			if (ntfs_valid_descr((char*)attr,attrsz)) {
#if POSIXACLS
				pxdesc = linux_permissions_posix(attr,isdir);
				if (pxdesc)
					mode = pxdesc->mode;
				else
					mode = 0;
#else
				mode = linux_permissions(attr,isdir);
#endif
				attrib = ntfs_get_file_attributes(ntfs_context,fullname);
				if (opt_v >= 2) {
					level = (opt_b ? 4 : 0);
					showheader(attr,level);
					showusid(attr,level);
					showgsid(attr,level);
					showdacl(attr,isdir,level);
					showsacl(attr,isdir,level);
				}
				if (attrib != INVALID_FILE_ATTRIBUTES)
					printf("Windows attrib : 0x%x\n",attrib);
				uid = linux_owner(attr);
				gid = linux_group(attr);
				if (opt_b) {
					printf("# Interpreted Unix owner %d, group %d, mode 0%03o\n",
						(int)uid,(int)gid,mode);
				} else {
					printf("Interpreted Unix owner %d, group %d, mode 0%03o\n",
						(int)uid,(int)gid,mode);
				}
#if POSIXACLS
				if (pxdesc) {
					if (!opt_b
					    && (pxdesc->defcnt
					       || (pxdesc->tagsset
						   & (POSIX_ACL_USER
							| POSIX_ACL_GROUP
							| POSIX_ACL_MASK))))
						showposix(pxdesc);
					free(pxdesc);
				}
#endif
				if ((opt_r || opt_b) && (securindex < MAXSECURID)
				    && (securindex > 0) && psecurdata) {
					psecurdata->filecount++;
					psecurdata->mode = mode;
				}
			} else {
				printf("** Descriptor fails sanity check\n");
				errors++;
			}
		}
	} else
		if (securindex > 0) {
			if (securdata[securindex >> SECBLKSZ]) {
				psecurdata = &securdata[securindex >> SECBLKSZ]
					[securindex & ((1 << SECBLKSZ) - 1)];
				psecurdata->filecount++;
				if (opt_b || opt_r) {
					if (!opt_b && !opt_v)
						printf("%s %s\n",(isdir ? "Directory" : "File"),fullname);
					printf("Security key : 0x%x mode %03o (already displayed)\n",
						securindex,psecurdata->mode);
					if (attrib != INVALID_FILE_ATTRIBUTES)
						printf("Windows attrib : 0x%x\n",attrib);
				} else {
					printf("%s %s",(isdir ? "Directory" : "File"),fullname);
					printf(" : key 0x%x\n",securindex);
				}
				if ((opt_a || opt_b) && opt_v
				    && psecurdata && psecurdata->attr) {
					printf("# %s %s hash 0x%lx\n",(isdir ? "Directory" : "File"),
						fullname,
						(unsigned long)hash((le32*)psecurdata->attr,
							ntfs_attr_size(psecurdata->attr)));
				}
			}
		} else {
			if (!opt_v && !opt_b)
				printf("%s %s",(isdir ? "Directory" : "File"),fullname);
			printf("   (Failed)\n");
			printf("** Could not get security data of %s, partsz %d\n",
				fullname,partsz);
			printerror(stdout);
			errors++;
		}
}

BOOL recurseshow(const char *path)
{
	struct CALLBACK dircontext;
	struct LINK *current;
	BOOL isdir;
	BOOL err;

	err = FALSE;
	dircontext.head = (struct LINK*)NULL;
	dircontext.dir = path;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, &dircontext);
	if (isdir) {
		showfull(path,TRUE);
		if (opt_v) {
			if (opt_b)
				printf("#\n#\n");
			else
				printf("\n\n");
		}
		while (dircontext.head) {
			current = dircontext.head;
			if (recurseshow(current->name)) err = TRUE;
			dircontext.head = dircontext.head->next;
			free(current);
		}
	} else
		if (errno == ENOTDIR) {
			showfull(path,FALSE);
			if (opt_v) {
				if (opt_b)
					printf("#\n#\n");
				else
					printf("\n\n");
			}
		} else {
			printf("** Could not access %s\n",path);
			printerror(stdout);
			errors++;
			err = TRUE;
		}
	return (!err);
}


BOOL singleshow(const char *path)
{
	BOOL isdir;
	BOOL err;

	err = FALSE;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, (struct CALLBACK*)NULL);
	if (isdir || (errno == ENOTDIR))
		showfull(path,isdir);
	else {
		printf("** Could not access %s\n",path);
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (err);
}

#ifdef HAVE_SETXATTR

static ssize_t ntfs_getxattr(const char *path, const char *name, void *value, size_t size)
{
#if defined(__APPLE__) || defined(__DARWIN__)
    return getxattr(path, name, value, size, 0, 0);
#else
    return getxattr(path, name, value, size);
#endif
}

/*
 *		   Display all the parameters associated to a mounted file
 */

void showmounted(const char *fullname)
{

	static char attr[MAXATTRSZ];
	struct stat st;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif
	BOOL mapped;
	int attrsz;
	int mode;
	uid_t uid;
	gid_t gid;
	u32 attrib;
	int level;
	BOOL isdir;

	if (!stat(fullname,&st)) {
		isdir = S_ISDIR(st.st_mode);
		printf("%s ",(isdir ? "Directory" : "File"));
		printname(stdout,fullname);
		printf("\n");

		attrsz = ntfs_getxattr(fullname,"system.ntfs_acl",attr,MAXATTRSZ);
		if (attrsz > 0) {
			if (opt_v) {
				hexdump(attr,attrsz,8);
				printf("Computed hash : 0x%08lx\n",
					(unsigned long)hash((le32*)attr,attrsz));
			}
			if (ntfs_getxattr(fullname,"system.ntfs_attrib",&attrib,4) != 4) {
				printf("** Could not get file attrib\n");
				errors++;
			} else
				printf("Windows attrib : 0x%x\n",(int)attrib);
			if (ntfs_valid_descr(attr,attrsz)) {
				mapped = !local_build_mapping(context.mapping,fullname);
#if POSIXACLS
				if (mapped) {
					pxdesc = linux_permissions_posix(attr,isdir);
					if (pxdesc)
						mode = pxdesc->mode;
					else
						mode = 0;
				} else {
					pxdesc = (struct POSIX_SECURITY*)NULL;
					mode = linux_permissions(attr,isdir);
					printf("No user mapping : "
						"cannot display the Posix ACL\n");
				}
#else
				mode = linux_permissions(attr,isdir);
#endif
				if (opt_v >= 2) {
					level = (opt_b ? 4 : 0);
					showheader(attr,level);
					showusid(attr,level);
					showgsid(attr,level);
					showdacl(attr,isdir,level);
					showsacl(attr,isdir,level);
				}
				if (mapped) {
					uid = linux_owner(attr);
					gid = linux_group(attr);
					printf("Interpreted Unix owner %d, group %d, mode 0%03o\n",
						(int)uid,(int)gid,mode);
				} else {
					printf("Interpreted Unix mode 0%03o (owner and group are unmapped)\n",
						mode);
				}
#if POSIXACLS
				if (pxdesc) {
					if ((pxdesc->defcnt
						|| (pxdesc->tagsset
						    & (POSIX_ACL_USER
							| POSIX_ACL_GROUP
							| POSIX_ACL_MASK))))
						showposix(pxdesc);
#if USESTUBS
					stdfree(pxdesc); /* allocated within library */
#else
					free(pxdesc);
#endif
				}
				if (mapped)
					ntfs_free_mapping(context.mapping);
#endif
			} else
				printf("Descriptor fails sanity check\n");
		} else {
			printf("** Could not get the NTFS ACL, check whether file is on NTFS\n");
			errors++;
		}
	} else
		printf("%s not found\n",fullname);
}

#else /* HAVE_SETXATTR */

void showmounted(const char *fullname __attribute__((unused)))
{
	fprintf(stderr,"Not possible on this configuration\n");
}

#endif /* HAVE_SETXATTR */

#if POSIXACLS

BOOL recurseset_posix(const char *path, const struct POSIX_SECURITY *pxdesc)
{
	struct CALLBACK dircontext;
	struct LINK *current;
	BOOL isdir;
	BOOL err;

	err = FALSE;
	dircontext.head = (struct LINK*)NULL;
	dircontext.dir = path;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, &dircontext);
	if (isdir) {
		err = !setfull_posix(path,pxdesc,TRUE);
		if (err) {
			printf("** Failed to update %s\n",path);
			printerror(stdout);
			errors++;
		} else {
			if (opt_b)
				printf("#\n#\n");
			else
				printf("\n\n");
			while (dircontext.head) {
				current = dircontext.head;
				recurseset_posix(current->name,pxdesc);
				dircontext.head = dircontext.head->next;
				free(current);
			}
		}
	} else
		if (errno == ENOTDIR) {
			err = !setfull_posix(path,pxdesc,FALSE);
			if (err) {
				printf("** Failed to update %s\n",path);
				printerror(stdout);
				errors++;
			}
		} else {
			printf("** Could not access %s\n",path);
			printerror(stdout);
			errors++;
			err = TRUE;
		}
	return (!err);
}

#else

BOOL recurseset(const char *path, int mode)
{
	struct CALLBACK dircontext;
	struct LINK *current;
	BOOL isdir;
	BOOL err;

	err = FALSE;
	dircontext.head = (struct LINK*)NULL;
	dircontext.dir = path;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, &dircontext);
	if (isdir) {
		setfull(path,mode,TRUE);
		if (opt_b)
			printf("#\n#\n");
		else
			printf("\n\n");
		while (dircontext.head) {
			current = dircontext.head;
			recurseset(current->name,mode);
			dircontext.head = dircontext.head->next;
			free(current);
		}
	} else
		if (errno == ENOTDIR)
			setfull(path,mode,FALSE);
		else {
			printf("** Could not access %s\n",path);
			printerror(stdout);
			errors++;
			err = TRUE;
		}
	return (!err);
}

#endif

#if POSIXACLS

BOOL singleset_posix(const char *path, const struct POSIX_SECURITY *pxdesc)
{
	BOOL isdir;
	BOOL err;

	err = FALSE;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, (struct CALLBACK*)NULL);
	if (isdir || (errno == ENOTDIR)) {
		err = !setfull_posix(path,pxdesc,isdir);
		if (err) {
			printf("** Failed to update %s\n",path);
			printerror(stdout);
			errors++;
		}
	} else {
		printf("** Could not access %s\n",path);
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (!err);
}

#endif

BOOL singleset(const char *path, int mode)
{
	BOOL isdir;
	BOOL err;

	err = FALSE;
	isdir = ntfs_read_directory(ntfs_context, path,
			callback, (struct CALLBACK*)NULL);
	if (isdir || (errno == ENOTDIR))
		setfull(path,mode,isdir);
	else {
		printf("** Could not access %s\n",path);
		printerror(stdout);
		errors++;
		err = TRUE;
	}
	return (!err);
}

int callback(struct CALLBACK *dircontext, char *ntfsname,
	int length, int type,
	long long pos  __attribute__((unused)), u64 mft_ref __attribute__((unused)),
	unsigned int dt_type __attribute__((unused)))
{
	struct LINK *linkage;
	char *name;
	int newlth;
	int size;

	size = utf8size(ntfsname,length);
	if (dircontext
	    && (type != 2)     /* 2 : dos name (8+3) */
	    && (size > 0)      /* chars convertible to utf8 */
	    && ((length > 2)
		|| (ntfsname[0] != '.')
		|| (ntfsname[1] != '\0')
		|| ((ntfsname[2] || ntfsname[3])
		   && ((ntfsname[2] != '.') || (ntfsname[3] != '\0'))))) {
		linkage = (struct LINK*)malloc(sizeof(struct LINK) 
				+ strlen(dircontext->dir)
				+ size + 2);
		if (linkage) {
		/* may find ".fuse_hidden*" files */
		/* recommendation is not to hide them, so that */
		/* the user has a clue to delete them */
			strcpy(linkage->name,dircontext->dir);
			if (linkage->name[strlen(linkage->name) - 1] != '/')
				strcat(linkage->name,"/");
			name = &linkage->name[strlen(linkage->name)];
			newlth = makeutf8(name,ntfsname,length);
			name[newlth] = 0;
			linkage->next = dircontext->head;
			dircontext->head = linkage;
		}
	}
	return (0);
}

#endif

#ifdef WIN32

/*
 *		 Backup security descriptors in a directory tree (Windows mode)
 */

BOOL backup(const char *root)
{
	BOOL err;
	time_t now;
	const char *txtime;

	now = time((time_t*)NULL);
	txtime = ctime(&now);
	printf("#\n# Recursive ACL collection on %s#\n",txtime);
	err = recurseshow(root);
	return (err);
}

#else

/*
 *		 Backup security descriptors in a directory tree (Linux mode)
 */

BOOL backup(const char *volume, const char *root)
{
	BOOL err;
	int count;
	int i,j;
	time_t now;
	const char *txtime;

	now = time((time_t*)NULL);
	txtime = ctime(&now);
	if (!getuid() && open_security_api()) {
		if (open_volume(volume,MS_RDONLY)) {
			printf("#\n# Recursive ACL collection on %s#\n",txtime);
			err = recurseshow(root);
			count = 0;
			for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
				if (securdata[i])
					for (j=0; j<(1 << SECBLKSZ); j++)
						if (securdata[i][j].filecount) {
							count++;
						}
			printf("# %d security keys\n",count);
			close_volume(volume);
		} else {
			fprintf(stderr,"Could not open volume %s\n",volume);
			printerror(stdout);
			err = TRUE;
		}
		close_security_api();
	} else {
		if (getuid())
			fprintf(stderr,"This is only possible as root\n");
		else
			fprintf(stderr,"Could not open security API\n");
		err = TRUE;
	}
	return (err);
}

#endif

#ifdef WIN32

/*
 *		 List security descriptors in a directory tree (Windows mode)
 */

BOOL listfiles(const char *root)
{
	BOOL err;

	if (opt_r) {
		printf("\nRecursive file check\n");
		err = recurseshow(root);
	} else
		err = singleshow(root);
	return (err);
}

#else

/*
 *		 List security descriptors in a directory tree (Linux mode)
 */

BOOL listfiles(const char *volume, const char *root)
{
	BOOL err;
	int i,j;
	int count;

	if (!getuid() && open_security_api()) {
		if (open_volume(volume,MS_RDONLY)) {
			if (opt_r) {
				printf("\nRecursive file check\n");
				err = recurseshow(root);
				printf("Summary\n");
				count = 0;
				for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
					if (securdata[i])
						for (j=0; j<(1 << SECBLKSZ); j++)
							if (securdata[i][j].filecount) {
								printf("Key 0x%x : %d files, mode 0%03o\n",
									i*(1 << SECBLKSZ)+j,securdata[i][j].filecount,
									securdata[i][j].mode);
								count++;
							}
				printf("%d security keys\n",count);
			} else
				err = singleshow(root);
			close_volume(volume);
		} else {
			fprintf(stderr,"Could not open volume %s\n",volume);
			printerror(stdout);
			err = TRUE;
		}
		close_security_api();
	} else {
		if (getuid())
			fprintf(stderr,"This is only possible as root\n");
		else
			fprintf(stderr,"Could not open security API\n");
		err = TRUE;
	}
	return (err);
}

#endif

#ifndef WIN32

/*
 *		Check whether a SDS entry is valid
 */

BOOL valid_sds(const char *attr, unsigned int offset,
		unsigned int entrysz, unsigned int size, u32 prevkey,
		BOOL second)
{
	BOOL unsane;
	u32 comphash;
	u32 key;

	unsane = FALSE;
	if (!get4l(attr,0) && !get4l(attr,4)) {
		printf("Entry at 0x%lx was deleted\n",(long)offset);
	} else {
		if ((ntfs_attr_size(&attr[20]) + 20) > entrysz) {
			printf("** Entry is truncated (expected size %ld)\n",
				(long)ntfs_attr_size(&attr[20] + 20));
			unsane = TRUE;
			errors++;
		}
		if ((ntfs_attr_size(&attr[20]) + 20) < entrysz) {
			printf("** Extra data appended to entry (expected size %ld)\n",
				(long)ntfs_attr_size(&attr[20]) + 20);
			warnings++;
		}
		if (!unsane && !ntfs_valid_descr((const char*)&attr[20],size)) {
			printf("** General sanity check has failed\n");
			unsane = TRUE;
			errors++;
		}
		if (!unsane) {
			comphash = hash((const le32*)&attr[20],entrysz-20);
			if ((u32)get4l(attr,0) == comphash) {
				if (opt_v >= 2)
					printf("Hash	 0x%08lx (correct)\n",
						(unsigned long)comphash);
			} else {
				printf("** hash  0x%08lx (computed : 0x%08lx)\n",
					(unsigned long)get4l(attr,0),
					(unsigned long)comphash);
				unsane = TRUE;
				errors++;
			}
		}
		if (!unsane) {
			if ((second ? get8l(attr,8) + 0x40000 : get8l(attr,8)) == offset) {
				if (opt_v >= 2)
					printf("Offset	 0x%lx (correct)\n",(long)offset);
			} else {
				printf("** offset  0x%llx (expected : 0x%llx)\n",
					(long long)get8l(attr,8),
					(long long)(second ? get8l(attr,8) - 0x40000 : get8l(attr,8)));
//				unsane = TRUE;
				errors++;
			}
		}
		if (!unsane) {
			key = get4l(attr,4);
			if (opt_v >= 2)
				printf("Key	 0x%x\n",(int)key);
			if (key) {
				if (key <= prevkey) {
					printf("** Unordered key 0x%lx after 0x%lx\n",
						(long)key,(long)prevkey);
					unsane = TRUE;
					errors++;
				}
			}
		}
	}
	return (!unsane);
}

/*
 *		Check whether a SDS entry is consistent with other known data
 *	and store current data for subsequent checks
 */

int consist_sds(const char *attr, unsigned int offset,
		unsigned int entrysz, BOOL second)
{
	int errcnt;
	u32 key;
	u32 comphash;
	struct SECURITY_DATA *psecurdata;

	errcnt = 0;
	key = get4l(attr,4);
	if ((key > 0) && (key < MAXSECURID)) {
		printf("Valid entry at 0x%lx for key 0x%lx\n",
			(long)offset,(long)key);
		if (!securdata[key >> SECBLKSZ])
			newblock(key);
		if (securdata[key >> SECBLKSZ]) {
			psecurdata = &securdata[key >> SECBLKSZ][key & ((1 << SECBLKSZ) - 1)];
			comphash = hash((const le32*)&attr[20],entrysz-20);
			if (psecurdata->flags & INSDS1) {
				if (psecurdata->hash != comphash) {
					printf("** Different hash values : $SDS-1 0x%08lx $SDS-2 0x%08lx\n",
						(unsigned long)psecurdata->hash,
						(unsigned long)comphash);
					errcnt++;
					errors++;
				}
				if (psecurdata->offset != get8l(attr,8)) {
					printf("** Different offsets : $SDS-1 0x%llx $SDS-2 0x%llx\n",
						(long long)psecurdata->offset,(long long)get8l(attr,8));
					errcnt++;
					errors++;
				}
				if (psecurdata->length != get4l(attr,16)) {
					printf("** Different lengths : $SDS-1 0x%lx $SDS-2 0x%lx\n",
						(long)psecurdata->length,(long)get4l(attr,16));
					errcnt++;
					errors++;
				}
			} else {
				if (second) {
					printf("** Entry was not present in $SDS-1\n");
					errcnt++;
					errors++;
				}
				psecurdata->hash = comphash;
				psecurdata->offset = get8l(attr,8);
				psecurdata->length = get4l(attr,16);
			}
			psecurdata->flags |= (second ? INSDS2 : INSDS1);
		}
	} else
		if (key || get4l(attr,0)) {
			printf("** Security_id 0x%x out of bounds\n",key);
			warnings++;
		}
	return (errcnt);
}


/*
 *		       Auditing of $SDS (Linux only)
 */

int audit_sds(BOOL second)
{
	static char attr[MAXATTRSZ + 20];
	BOOL isdir;
	BOOL done;
	BOOL unsane;
	u32 prevkey;
	int errcnt;
	int size;
	unsigned int entrysz;
	unsigned int entryalsz;
	unsigned int offset;
	int count;
	int deleted;
	int mode;

	if (second)
		printf("\nAuditing $SDS-2\n");
	else
		printf("\nAuditing $SDS-1\n");
	errcnt = 0;
	offset = (second ? 0x40000 : 0);
	count = 0;
	deleted = 0;
	done = FALSE;
	prevkey = 0;

	  /* get size of first record */

	size = ntfs_read_sds(ntfs_context,(char*)attr,20,offset);
	if (size != 20) {
		if ((size < 0) && (errno == ENOTSUP))
			printf("** There is no $SDS-%d in this volume\n",
							(second ? 2 : 1));
		else {
			printf("** Could not open $SDS-%d, size %d\n",
							(second ? 2 : 1),size);
			errors++;
			errcnt++;
		}
	} else
		do {
			entrysz = get4l(attr,16);
			entryalsz = ((entrysz - 1) | 15) + 1;
			if (entryalsz <= (MAXATTRSZ + 20)) {
				/* read next header in anticipation, to get its size */
				size = ntfs_read_sds(ntfs_context,
					(char*)&attr[20],entryalsz,offset + 20);
				if (opt_v)
					printf("\nAt offset 0x%lx got %lu bytes\n",(long)offset,(long)size);
			} else {
				printf("** Security attribute is too long (%ld bytes) - stopping\n",
					(long)entryalsz);
				errcnt++;
			}
			if ((entryalsz > (MAXATTRSZ + 20)) || (size < (int)(entrysz - 20)))
				done = TRUE;
			else {
				if (opt_v) {
					printf("Entry size %d bytes\n",entrysz);
					hexdump(&attr[20],size,8);
				}

				unsane = !valid_sds(attr,offset,entrysz,
					size,prevkey,second);
				if (!unsane) {
					if (!get4l(attr,0) && !get4l(attr,4))
						deleted++;
					else
						count++;
					errcnt += consist_sds(attr,offset,
						entrysz, second);
					if (opt_v >= 2) {
						isdir = guess_dir(&attr[20]);
						printf("Assuming %s descriptor\n",(isdir ? "directory" : "file"));
						showheader(&attr[20],0);
						showusid(&attr[20],0);
						showgsid(&attr[20],0);
						showdacl(&attr[20],isdir,0);
						showsacl(&attr[20],isdir,0);
						mode = linux_permissions(
						    &attr[20],isdir);
						printf("Interpreted Unix mode 0%03o\n",mode);
					}
					prevkey = get4l(attr,4);
				}
				if (!unsane) {
					memcpy(attr,&attr[entryalsz],20);
					offset += entryalsz;
					if (!get4l(attr,16)
					   || ((((offset - 1) | 0x3ffff) - offset + 1) < 20)) {
						if (second)
							offset = ((offset - 1) | 0x7ffff) + 0x40001;
						else
							offset = ((offset - 1) | 0x7ffff) + 1;
						if (opt_v)
							printf("Trying next SDS-%d block at offset 0x%lx\n",
								(second ? 2 : 1), (long)offset);
						size = ntfs_read_sds(ntfs_context,
							(char*)attr,20,offset);
						if (size != 20) {
							if (opt_v)
								printf("Assuming end of $SDS, got %d bytes\n",size);
							done = TRUE;
						}
					}
				} else {
					printf("** Sanity check failed - stopping there\n");
					errcnt++;
					errors++;
					done = TRUE;
				}
			}
		} while (!done);
	if (count || deleted || errcnt) {
		printf("%d valid and %d deleted entries in $SDS-%d\n",
				count,deleted,(second ? 2 : 1));
		printf("%d errors in $SDS-%c\n",errcnt,(second ? '2' : '1'));
	}
	return (errcnt);
}

/*
 *		Check whether a SII entry is sane
 */

BOOL valid_sii(const char *entry, u32 prevkey)
{
	BOOL valid;
	u32 key;

	valid = TRUE;
	key = get4l(entry,16);
	if (key <= prevkey) {
		printf("** Unordered key 0x%lx after 0x%lx\n",
			(long)key,(long)prevkey);
		valid = FALSE;
		errors++;
	}
	prevkey = key;
	if (get2l(entry,0) != 20) {
		printf("** offset %d (instead of 20)\n",(int)get2l(entry,0));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,2) != 20) {
		printf("** size %d (instead of 20)\n",(int)get2l(entry,2));
		valid = FALSE;
		errors++;
	}
	if (get4l(entry,4) != 0) {
		printf("** fill1 %d (instead of 0)\n",(int)get4l(entry,4));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,12) & 1) {
		if (get2l(entry,8) != 48) {
			printf("** index size %d (instead of 48)\n",(int)get2l(entry,8));
			valid = FALSE;
			errors++;
		}
	} else
		if (get2l(entry,8) != 40) {
			printf("** index size %d (instead of 40)\n",(int)get2l(entry,8));
			valid = FALSE;
			errors++;
		}
	if (get2l(entry,10) != 4) {
		printf("** index key size %d (instead of 4)\n",(int)get2l(entry,10));
		valid = FALSE;
		errors++;
	}
	if ((get2l(entry,12) & ~3) != 0) {
		printf("** flags 0x%x (instead of < 4)\n",(int)get2l(entry,12));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,14) != 0) {
		printf("** fill2 %d (instead of 0)\n",(int)get2l(entry,14));
		valid = FALSE;
		errors++;
	}
	if (get4l(entry,24) != key) {
		printf("** key 0x%x (instead of 0x%x)\n",
						(int)get4l(entry,24),(int)key);
		valid = FALSE;
		errors++;
	}
	return (valid);
}

/*
 *		Check whether a SII entry is consistent with other known data
 */

int consist_sii(const char *entry)
{
	int errcnt;
	u32 key;
	struct SECURITY_DATA *psecurdata;

	errcnt = 0;
	key = get4l(entry,16);
	if ((key > 0) && (key < MAXSECURID)) {
		printf("Valid entry for key 0x%lx\n",(long)key);
		if (!securdata[key >> SECBLKSZ])
			newblock(key);
		if (securdata[key >> SECBLKSZ]) {
			psecurdata = &securdata[key >> SECBLKSZ][key & ((1 << SECBLKSZ) - 1)];
			psecurdata->flags |= INSII;
			if (psecurdata->flags & (INSDS1 | INSDS2)) {
				if ((u32)get4l(entry,20) != psecurdata->hash) {
					printf("** hash 0x%x (instead of 0x%x)\n",
						(unsigned int)get4l(entry,20),
						(unsigned int)psecurdata->hash);
					errors++;
				}
				if (get8l(entry,28) != psecurdata->offset) {
					printf("** offset 0x%llx (instead of 0x%llx)\n",
						(long long)get8l(entry,28),
						(long long)psecurdata->offset);
					errors++;
				}
				if (get4l(entry,36) != psecurdata->length) {
					printf("** length 0x%lx (instead of %ld)\n",
						(long)get4l(entry,36),
						(long)psecurdata->length);
					errors++;
				}
			} else {
				printf("** Entry was not present in $SDS\n");
				errors++;
				psecurdata->hash = get4l(entry,20);
				psecurdata->offset = get8l(entry,28);
				psecurdata->length = get4l(entry,36);
				if (opt_v) {
					printf("   hash 0x%x\n",(unsigned int)psecurdata->hash);
					printf("   offset 0x%llx\n",(long long)psecurdata->offset);
					printf("   length %ld\n",(long)psecurdata->length);
				}
				errcnt++;
			}
		}
	} else {
		printf("** Security_id 0x%x out of bounds\n",key);
		warnings++;
	}
	return (errcnt);
}


/*
 *		       Auditing of $SII (Linux only)
 */

int audit_sii()
{
	char *entry;
	int errcnt;
	u32 prevkey;
	BOOL valid;
	BOOL done;
	int count;

	printf("\nAuditing $SII\n");
	errcnt = 0;
	count = 0;
	entry = (char*)NULL;
	prevkey = 0;
	done = FALSE;
	do {
		entry = (char*)ntfs_read_sii(ntfs_context,(void*)entry);
		if (entry) {
			valid = valid_sii(entry,prevkey);
			if (valid) {
				count++;
				errcnt += consist_sii(entry);
				prevkey = get4l(entry,16);
			} else
				errcnt++;
		} else
			if ((errno == ENOTSUP) && !prevkey)
				printf("** There is no $SII in this volume\n");
	} while (entry && !done);
	if (count || errcnt) {
		printf("%d valid entries in $SII\n",count);
		printf("%d errors in $SII\n",errcnt);
	}
	return (errcnt);
}

/*
 *		Check whether a SII entry is sane
 */

BOOL valid_sdh(const char *entry, u32 prevkey, u32 prevhash)
{
	BOOL valid;
	u32 key;
	u32 currhash;

	valid = TRUE;
	currhash = get4l(entry,16);
	key = get4l(entry,20);
	if ((currhash < prevhash)
		|| ((currhash == prevhash) && (key <= prevkey))) {
		printf("** Unordered hash and key 0x%x 0x%x after 0x%x 0x%x\n",
			(unsigned int)currhash,(unsigned int)key,
			(unsigned int)prevhash,(unsigned int)prevkey);
		valid = FALSE;
		errors++;
	}
	if ((opt_v >= 2) && (currhash == prevhash))
		printf("Hash collision (not an error)\n");

	if (get2l(entry,0) != 24) {
		printf("** offset %d (instead of 24)\n",(int)get2l(entry,0));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,2) != 20) {
		printf("** size %d (instead of 20)\n",(int)get2l(entry,2));
		valid = FALSE;
		errors++;
	}
	if (get4l(entry,4) != 0) {
		printf("** fill1 %d (instead of 0)\n",(int)get4l(entry,4));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,12) & 1) {
		if (get2l(entry,8) != 56) {
			printf("** index size %d (instead of 56)\n",(int)get2l(entry,8));
			valid = FALSE;
			errors++;
		}
	} else
		if (get2l(entry,8) != 48) {
			printf("** index size %d (instead of 48)\n",(int)get2l(entry,8));
			valid = FALSE;
			errors++;
		}
	if (get2l(entry,10) != 8) {
		printf("** index key size %d (instead of 8)\n",(int)get2l(entry,10));
		valid = FALSE;
		errors++;
	}
	if ((get2l(entry,12) & ~3) != 0) {
		printf("** flags 0x%x (instead of < 4)\n",(int)get2l(entry,12));
		valid = FALSE;
		errors++;
	}
	if (get2l(entry,14) != 0) {
		printf("** fill2 %d (instead of 0)\n",(int)get2l(entry,14));
		valid = FALSE;
		errors++;
	}
	if ((u32)get4l(entry,24) != currhash) {
		printf("** hash 0x%x (instead of 0x%x)\n",
			(unsigned int)get4l(entry,24),(unsigned int)currhash);
		valid = FALSE;
		errors++;
	}
	if (get4l(entry,28) != key) {
		printf("** key 0x%x (instead of 0x%x)\n",
			(int)get4l(entry,28),(int)key);
		valid = FALSE;
		errors++;
	}
	if (get4l(entry,44)
		&& (get4l(entry,44) != 0x490049)) {
		printf("** fill3 0x%lx (instead of 0 or 0x490049)\n",
			(long)get4l(entry,44));
		valid = FALSE;
		errors++;
	}
	return (valid);
}

/*
 *		Check whether a SDH entry is consistent with other known data
 */

int consist_sdh(const char *entry)
{
	int errcnt;
	u32 key;
	struct SECURITY_DATA *psecurdata;

	errcnt = 0;
	key = get4l(entry,20);
	if ((key > 0) && (key < MAXSECURID)) {
		printf("Valid entry for key 0x%lx\n",(long)key);
		if (!securdata[key >> SECBLKSZ])
			newblock(key);
		if (securdata[key >> SECBLKSZ]) {
			psecurdata = &securdata[key >> SECBLKSZ][key & ((1 << SECBLKSZ) - 1)];
			psecurdata->flags |= INSDH;
			if (psecurdata->flags & (INSDS1 | INSDS2 | INSII)) {
				if ((u32)get4l(entry,24) != psecurdata->hash) {
					printf("** hash 0x%x (instead of 0x%x)\n",
						(unsigned int)get4l(entry,24),
						(unsigned int)psecurdata->hash);
					errors++;
				}
				if (get8l(entry,32) != psecurdata->offset) {
					printf("** offset 0x%llx (instead of 0x%llx)\n",
						(long long)get8l(entry,32),
						(long long)psecurdata->offset);
					errors++;
				}
				if (get4l(entry,40) != psecurdata->length) {
					printf("** length %ld (instead of %ld)\n",
						(long)get4l(entry,40),
						(long)psecurdata->length);
					errors++;
				}
			} else {
				printf("** Entry was not present in $SDS nor in $SII\n");
				errors++;
				psecurdata->hash = get4l(entry,24);
				psecurdata->offset = get8l(entry,32);
				psecurdata->length = get4l(entry,40);
				if (opt_v) {
					printf("   offset 0x%llx\n",(long long)psecurdata->offset);
					printf("   length %ld\n",(long)psecurdata->length);
				}
				errcnt++;
			}
		}
	} else {
		printf("** Security_id 0x%x out of bounds\n",key);
		warnings++;
	}
	return (errcnt);
}

/*
 *		       Auditing of $SDH (Linux only)
 */

int audit_sdh()
{
	char *entry;
	int errcnt;
	int count;
	u32 prevkey;
	u32 prevhash;
	BOOL valid;
	BOOL done;

	printf("\nAuditing $SDH\n");
	count = 0;
	errcnt = 0;
	prevkey = 0;
	prevhash = 0;
	entry = (char*)NULL;
	done = FALSE;
	do {
		entry = (char*)ntfs_read_sdh(ntfs_context,(void*)entry);
		if (entry) {
			valid = valid_sdh(entry,prevkey,prevhash);
			if (valid) {
				count++;
				errcnt += consist_sdh(entry);
				prevhash = get4l(entry,16);
				prevkey = get4l(entry,20);
			} else
				errcnt++;
		} else
			if ((errno == ENOTSUP) && !prevkey)
				printf("** There is no $SDH in this volume\n");
	} while (entry && !done);
	if (count || errcnt) {
		printf("%d valid entries in $SDH\n",count);
		printf("%d errors in $SDH\n",errcnt);
	}
	return (errcnt);
}

/*
 *		Audit summary
 */

void audit_summary()
{
	int count;
	int flags;
	int cnt;
	int found;
	int i,j;

	count = 0;
	found = 0;
	if (opt_r) printf("Summary of security key use :\n");
	for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
		if (securdata[i])
			for (j=0; j<(1 << SECBLKSZ); j++) {
				flags = securdata[i][j].flags & (INSDS1 + INSDS2 + INSII + INSDH);
				if (flags) found++;
				if (flags
					&& (flags != (INSDS1 + INSDS2 + INSII + INSDH)))
					{
					if (!count && !opt_r)
						printf("\n** Keys not present in all files :\n");
					cnt = securdata[i][j].filecount;
					if (opt_r)
						printf("Key 0x%x used by %d %s, not in",
							i*(1 << SECBLKSZ)+j,cnt,
							(cnt > 1 ? "files" : "file"));
					else
						printf("Key 0x%x not in", i*(1 << SECBLKSZ)+j);
					if (!(flags & INSDS1))
						printf(" SDS-1");
					if (!(flags & INSDS2))
						printf(" SDS-2");
					if (!(flags & INSII))
						printf(" SII");
					if (!(flags & INSDH))
						printf(" SDH");
					printf("\n");
					count++;
				} else {
					cnt = securdata[i][j].filecount;
					if (opt_r && cnt)
						printf("Key 0x%x used by %d %s\n",
							i*(1 << SECBLKSZ)+j,cnt,
							(cnt > 1 ? "files" : "file"));
				}
			}
	if (found) {
		if (count)
			printf("%d keys not present in all lists\n",count);
		else
			printf("All keys are present in all lists\n");
	}
}

/*
 *		       Auditing (Linux only)
 */

BOOL audit(const char *volume)
{
	BOOL err;

	err = FALSE;
	if (!getuid() && open_security_api()) {
		if (open_volume(volume,MS_RDONLY)) {
			if (audit_sds(FALSE)) err = TRUE;
			if (audit_sds(TRUE)) err = TRUE;
			if (audit_sii()) err = TRUE;
			if (audit_sdh()) err = TRUE;
			if (opt_r) recurseshow("/");

			audit_summary();
			close_volume(volume);
		}
		else {
			fprintf(stderr,"Could not open volume %s\n",volume);
			printerror(stdout);
			err = TRUE;
		}
		close_security_api();
	}
	else {
		if (getuid())
			fprintf(stderr,"This is only possible as root\n");
		else fprintf(stderr,"Could not open security API\n");
		err = TRUE;
	}
	return (err);
}

#endif

#if POSIXACLS

/*
 *		Encode a Posix ACL string
 *	[d:]{ugmo}:uid[:perms],...
 */

struct POSIX_SECURITY *encode_posix_acl(const char *str)
{
	int acccnt;
	int defcnt;
	int i,k,l;
	int c;
	s32 id;
	u16 perms;
	u16 apermsset;
	u16 dpermsset;
	u16 tag;
	u16 tagsset;
	mode_t mode;
	BOOL defacl;
	BOOL dmask;
	BOOL amask;
	const char *p;
	struct POSIX_ACL *acl;
	struct POSIX_SECURITY *pxdesc;
	enum { PXBEGIN, PXTAG, PXTAG1, PXID, PXID1, PXID2,
		PXPERM, PXPERM1, PXPERM2, PXOCT, PXNEXT, PXEND, PXERR
	} state;

				/* raw evaluation of ACE count */
	p = str;
	amask = FALSE;
	dmask = FALSE;
	if (*p == 'd') {
		acccnt = 0;
		defcnt = 1;
	} else {
		if ((*p >= '0') && (*p <= '7'))
			acccnt = 0;
		else
			acccnt = 1;
		defcnt = 0;
	}
	while (*p)
		if (*p++ == ',') {
			if (*p == 'd') {
				defcnt++;
				if (p[1] && (p[2] == 'm'))
					dmask = TRUE;
			} else {
				acccnt++;
				if (*p == 'm')
					amask = TRUE;
			}
		}
		/* account for an implicit mask if none defined */
	if (acccnt && !amask)
		acccnt++;
	if (defcnt && !dmask)
		defcnt++;
	pxdesc = (struct POSIX_SECURITY*)malloc(sizeof(struct POSIX_SECURITY)
				+ (acccnt + defcnt)*sizeof(struct POSIX_ACE));
	if (pxdesc) {
		pxdesc->acccnt = acccnt;
		pxdesc->firstdef = acccnt;
		pxdesc->defcnt = defcnt;
		acl = &pxdesc->acl;
		p = str;
		state = PXBEGIN;
		id = 0;
		defacl = FALSE;
		mode = 0;
		apermsset = 0;
		dpermsset = 0;
		tag = 0;
		perms = 0;
		k = l = 0;
		c = *p++;
		while ((state != PXEND) && (state != PXERR)) {
			switch (state) {
			case PXBEGIN :
				if (c == 'd') {
					defacl = TRUE;
					state = PXTAG1;
					break;
				} else
					if ((c >= '0') && (c <= '7')) {
						mode = c - '0';
						state = PXOCT;
						break;
					}
				defacl = FALSE;
				/* fall through */
			case PXTAG :
				switch (c) {
				case 'u' :
					tag = POSIX_ACL_USER;
					state = PXID;
					break;
				case 'g' :
					tag = POSIX_ACL_GROUP;
					state = PXID;
					break;
				case 'o' :
					tag = POSIX_ACL_OTHER;
					state = PXID;
					break;
				case 'm' :
					tag = POSIX_ACL_MASK;
					state = PXID;
					break;
				default :
					state = PXERR;
					break;
				}
				break;
			case PXTAG1 :
				if (c == ':')
					state = PXTAG;
				else
					state = PXERR;
				break;
			case PXID :
				if (c == ':') {
					if ((tag == POSIX_ACL_OTHER)
					   || (tag == POSIX_ACL_MASK))
						state = PXPERM;
					else
						state = PXID1;
				} else
					state = PXERR;
				break;
			case PXID1 :
				if ((c >= '0') && (c <= '9')) {
					id = c - '0';
					state = PXID2;
				} else
					if (c == ':') {
						id = -1;
						if (tag == POSIX_ACL_USER)
							tag = POSIX_ACL_USER_OBJ;
						if (tag == POSIX_ACL_GROUP)
							tag = POSIX_ACL_GROUP_OBJ;
						state = PXPERM1;
					} else
						state = PXERR;
				break;
			case PXID2 :
				if ((c >= '0') && (c <= '9'))
					id = 10*id + c - '0';
				else
					if (c == ':')
						state = PXPERM1;
					else
						state = PXERR;
				break;
			case PXPERM :
				if (c == ':') {
					id = -1;
					state = PXPERM1;
				} else
					state = PXERR;
				break;
			case PXPERM1 :
				if ((c >= '0') && (c <= '7')) {
					perms = c - '0';
					state = PXNEXT;
					break;
				}
				state = PXPERM2;
				perms = 0;
				/* fall through */
			case PXPERM2 :
				switch (c) {
				case 'r' :
					perms |= POSIX_PERM_R;
					break;
				case 'w' :
					perms |= POSIX_PERM_W;
					break;
				case 'x' :
					perms |= POSIX_PERM_X;
					break;
				case ',' :
				case '\0' :
					if (defacl) {
						i = acccnt + l++;
						dpermsset |= perms;
					} else {
						i = k++;
						apermsset |= perms;
					}
					acl->ace[i].tag = tag;
					acl->ace[i].perms = perms;
					acl->ace[i].id = id;
					if (c == '\0')
						state = PXEND;
					else
						state = PXBEGIN;
					break;
				}
				break;
			case PXNEXT :
				if (!c || (c == ',')) {
					if (defacl) {
						i = acccnt + l++;
						dpermsset |= perms;
					} else {
						i = k++;
						apermsset |= perms;
					}
					acl->ace[i].tag = tag;
					acl->ace[i].perms = perms;
					acl->ace[i].id = id;
					if (c == '\0')
						state = PXEND;
					else
						state = PXBEGIN;
				} else
					state = PXERR;
				break;
			case PXOCT :
				if ((c >= '0') && (c <= '7'))
					mode = (mode << 3) + c - '0';
				else
					if (c == '\0')
						state = PXEND;
					else
						state = PXBEGIN;
				break;
			default :
				break;
			}
			c = *p++;
		}
			/* insert default mask if none defined */
		if (acccnt && !amask) {
			i = k++;
			acl->ace[i].tag = POSIX_ACL_MASK;
			acl->ace[i].perms = apermsset;
			acl->ace[i].id = -1;
		}
		if (defcnt && !dmask) {
			i = acccnt + l++;
			acl->ace[i].tag = POSIX_ACL_MASK;
			acl->ace[i].perms = dpermsset;
			acl->ace[i].id = -1;
		}
			/* compute the mode and tagsset */
		tagsset = 0;
		for (i=0; i<acccnt; i++)
			tagsset |= acl->ace[i].tag;
			switch (acl->ace[i].tag) {
			case POSIX_ACL_USER_OBJ :
				mode |= acl->ace[i].perms << 6;
				break;
			case POSIX_ACL_GROUP_OBJ :
				mode |= acl->ace[i].perms << 3;
				break;
			case POSIX_ACL_OTHER :
				mode |= acl->ace[i].perms;
				break;
			default :
				break;
			}
		pxdesc->mode = mode;
		pxdesc->tagsset = tagsset;
		pxdesc->acl.version = POSIX_VERSION;
		pxdesc->acl.flags = 0;
		pxdesc->acl.filler = 0;
		if (state != PXERR)
			ntfs_sort_posix(pxdesc);
showposix(pxdesc);
		if ((state == PXERR)
		   || (k != acccnt)
		   || (l != defcnt)
                   || !ntfs_valid_posix(pxdesc)) {
			if (~pxdesc->tagsset
			    & (POSIX_ACL_USER_OBJ | POSIX_ACL_GROUP_OBJ | POSIX_ACL_OTHER))
				fprintf(stderr,"User, group or other permissions missing\n");
			else
				fprintf(stderr,"Bad ACL description\n");
			free(pxdesc);
			pxdesc = (struct POSIX_SECURITY*)NULL;
		} else
			if (opt_v >= 2) {
				printf("Interpreted input description :\n");
				showposix(pxdesc);
			}
	} else
		errno = ENOMEM;
	return (pxdesc);
}

#endif /* POSIXACLS */


int getoptions(int argc, char *argv[])
{
	int xarg;
	int narg;
	const char *parg;
	BOOL err;

	opt_a = FALSE;
	opt_b = FALSE;
   opt_e = FALSE;
	opt_h = FALSE;
#if FORCEMASK
	opt_m = FALSE;
#endif
	opt_r = FALSE;
	opt_s = FALSE;
#if SELFTESTS & !USESTUBS
	opt_t = FALSE;
#endif
	opt_v = 0;
	xarg = 1;
	err = FALSE;
	while ((xarg < argc) && (argv[xarg][0] == '-')) {
		parg = argv[xarg++];
		while (*++parg)
			switch (*parg)
				{
#ifndef WIN32
				case 'a' :
					opt_a = TRUE;
					break;
#endif
				case 'b' :
					opt_b = TRUE;
					break;
				case 'e' :
					opt_e = TRUE;
					break;
				case 'h' :
					opt_h = TRUE;
					break;
#if FORCEMASK
				case 'm' :
					opt_m = TRUE;
					break;
#endif
				case 'r' :
				case 'R' :
					opt_r = TRUE;
					break;
				case 's' :
					opt_s = TRUE;
					break;
#if SELFTESTS & !USESTUBS
				case 't' :
					opt_t = TRUE;
					break;
#endif
				case 'v' :
					opt_v++;
					break;
				default :
					fprintf(stderr,"Invalid option -%c\n",*parg);
					err = TRUE;
			}
	}
	narg = argc - xarg;
#ifdef WIN32
	if (   ((opt_h || opt_s) && (narg > 1))
	    || ((opt_r || opt_b) && ((narg < 1) || (narg > 2)))
#if SELFTESTS & !USESTUBS
	    || (opt_t && (narg > 0))
#endif
	    || (opt_e && !opt_s)
	    || (!opt_h && !opt_r && !opt_b && !opt_s
#if SELFTESTS & !USESTUBS
			&& !opt_t
#endif
			&& ((narg < 1) || (narg > 2))))

		err = TRUE;
	if (err) {
		xarg = 0;
		fprintf(stderr,"Usage:\n");
#if SELFTESTS & !USESTUBS
		fprintf(stderr,"   secaudit -t\n");
		fprintf(stderr,"	run self-tests\n");
#endif
		fprintf(stderr,"   secaudit -h [file]\n");
		fprintf(stderr,"	display security descriptors within file\n");
		fprintf(stderr,"   secaudit [-v] file\n");
		fprintf(stderr,"	display the security parameters of file\n");
		fprintf(stderr,"   secaudit -r[v] directory\n");
		fprintf(stderr,"	display the security parameters of files in directory\n");
		fprintf(stderr,"   secaudit -b[v] directory\n");
		fprintf(stderr,"        backup the security parameters of files in directory\n");
		fprintf(stderr,"   secaudit -s[ev] [backupfile]\n");
		fprintf(stderr,"        set the security parameters as indicated in backup file\n");
		fprintf(stderr,"        with -e also set extra parameters (Windows attrib)\n");
		fprintf(stderr,"   secaudit perms file\n");
		fprintf(stderr,"	set the security parameters of file to perms\n");
		fprintf(stderr,"   secaudit -r[v] perms directory\n");
		fprintf(stderr,"	set the security parameters of files in directory to perms\n");
#if POSIXACLS
		fprintf(stderr,"   Note: perms can be an octal mode or a Posix ACL description\n");
#else
		fprintf(stderr,"   Note: perms is an octal mode\n");
#endif
		fprintf(stderr,"          -v is for verbose, -vv for very verbose\n");
	}
#else
	if (   (opt_h && (narg > 1))
	    || (opt_a && (narg != 1))
	    || ((opt_r || opt_b || opt_s) && ((narg < 1) || (narg > 3)))
#if SELFTESTS & !USESTUBS
	    || (opt_t && (narg > 0))
#endif
	    || (opt_e && !opt_s)
	    || (!opt_h && !opt_a && !opt_r && !opt_b && !opt_s
#if SELFTESTS & !USESTUBS
		&& !opt_t
#endif
#ifdef HAVE_SETXATTR
		&& ((narg < 1) || (narg > 3))))
#else
		&& ((narg < 2) || (narg > 3))))
#endif
		err = TRUE;
	if (err) {
		xarg = 0;
		fprintf(stderr,"Usage:\n");
#if SELFTESTS & !USESTUBS
		fprintf(stderr,"   secaudit -t\n");
		fprintf(stderr,"	run self-tests\n");
#endif
		fprintf(stderr,"   secaudit -h [file]\n");
		fprintf(stderr,"	display security descriptors within file\n");
		fprintf(stderr,"   secaudit -a[rv] volume\n");
		fprintf(stderr,"	audit the volume\n");
		fprintf(stderr,"   secaudit [-v] volume file\n");
		fprintf(stderr,"	display the security parameters of file\n");
		fprintf(stderr,"   secaudit -r[v] volume directory\n");
		fprintf(stderr,"	display the security parameters of files in directory\n");
		fprintf(stderr,"   secaudit -b[v] volume directory\n");
		fprintf(stderr,"        backup the security parameters of files in directory\n");
		fprintf(stderr,"   secaudit -s[ev] volume [backupfile]\n");
		fprintf(stderr,"        set the security parameters as indicated in backup file\n");
		fprintf(stderr,"        with -e also set extra parameters (Windows attrib)\n");
		fprintf(stderr,"   secaudit volume perms file\n");
		fprintf(stderr,"	set the security parameters of file to perms\n");
		fprintf(stderr,"   secaudit -r[v] volume perms directory\n");
		fprintf(stderr,"	set the security parameters of files in directory to perms\n");
#ifdef HAVE_SETXATTR
		fprintf(stderr," special case, does not require being root :\n");
		fprintf(stderr,"   secaudit [-v] mounted-file\n");
		fprintf(stderr,"	display the security parameters of a mounted file\n");
#endif
#if POSIXACLS
		fprintf(stderr,"   Note: perms can be an octal mode or a Posix ACL description\n");
#else
		fprintf(stderr,"   Note: perms is an octal mode\n");
#endif
		fprintf(stderr,"          -v is for verbose, -vv for very verbose\n");
	}
#endif
	if ((sizeof(SID) != 12) && !err) {
		fprintf(stderr,"Possible alignment problem, check your compiler options\n");
		err = TRUE;
		xarg = 0;
	}
	return (xarg);
}

/*
 *		Memory allocation with checks
 */

#undef malloc
#undef calloc
#undef free
#undef isalloc

void dumpalloc(const char *txt)
{
	struct CHKALLOC *q;

	if (firstalloc) {
		printf("alloc table at %s\n",txt);
		for (q=firstalloc; q; q=q->next)
			printf("%08lx : %u bytes at %08lx allocated at %s line %d\n",
				(long)q,(unsigned int)q->size,
				(long)q->alloc,q->file,q->line);
	}
}

void *chkmalloc(size_t size, const char *file, int line)
{
	void *p;
	struct CHKALLOC *q;

	p = (void*)malloc(size+1);
	if (p) {
		((unsigned char*)p)[size] = 0xaa;
		q = (struct CHKALLOC*)malloc(sizeof(struct CHKALLOC));
		if (q) {
			q->next = firstalloc;
			q->alloc = p;
			q->size = size;
			q->file = file;
			q->line = line;
			firstalloc = q;
		}
	}
	return (p);
}

void *chkcalloc(size_t cnt, size_t size, const char *file, int line)
{
	return (chkmalloc(cnt*size,file,line));
}

void chkfree(void *p, const char *file, int line)
{
	struct CHKALLOC *q;
	struct CHKALLOC *r;

	if (p) {
		if (firstalloc && (firstalloc->alloc == p)) {
			r = firstalloc;
			firstalloc = firstalloc->next;
		} else {
			q = firstalloc;
			if (q)
				while (q->next && (q->next->alloc != p))
					q = q->next;
			if (q && q->next) {
				r = q->next;
				q->next = r->next;
			} else {
				r = (struct CHKALLOC*)NULL;
				printf("** freeing unallocated memory in %s line %d\n",file,line);
				if (!isatty(1))
					fprintf(stderr,"** freeing unallocated memory in %s line %d\n",file,line);
			}
		}
		if (r) {
			if (((unsigned char*)p)[r->size] != 0xaa) {
				printf("** memory corruption, alloc in %s line %d release in %s %d\n",
					r->file,r->line,file,line);
				if (!isatty(1))
					fprintf(stderr,"** memory corruption, alloc in %s line %d release in %s %d\n",
						r->file,r->line,file,line);
			}
			memset(p,0xaa,r->size);
			free(r);
			free(p);
		}
	}
}

void stdfree(void *p)
{
	free(p);
}

BOOL chkisalloc(void *p, const char *file, int line)
{
	struct CHKALLOC *q;

	if (p) {
		q = firstalloc;
		while (q && (q->alloc != p))
			q = q->next;
	} else
		q = (struct CHKALLOC*)NULL;
	if (!p || !q) {
		printf("error in %s %d : 0x%lx not allocated\n",file,line,(long)p);
	}
	return (p && q);
}




#ifdef WIN32

/*
 *		 Windows version
 */

main(argc,argv)
int argc;
char *argv[];
{
	FILE *fd;
	int xarg;
	int mode;
	unsigned int size;
	BOOL cmderr;
	char *filename;
	const char *p;
	int i;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif

	printf("%s\n",BANNER);
	cmderr = FALSE;
	errors = 0;
	warnings = 0;
	xarg = getoptions(argc,argv);
	if (xarg) {
		for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
			securdata[i] = (struct SECURITY_DATA*)NULL;
#if POSIXACLS
		context.mapping[MAPUSERS] = (struct MAPPING*)NULL;
		context.mapping[MAPGROUPS] = (struct MAPPING*)NULL;
#endif
		firstalloc = (struct CHKALLOC*)NULL;
		mappingtype = MAPNONE;
		switch (argc - xarg) {
		case 0 :
			if (opt_h)
				showhex(stdin);
			else
				if (opt_s)
					restore(stdin);
#if SELFTESTS & !USESTUBS
			if (opt_t)
				selftests();
#endif
			break;
		case 1 :
			if (opt_h || opt_s) {
				fd = fopen(argv[xarg],"r");
				if (fd) {
					if (opt_h)
						showhex(fd);
					else
						restore(fd);
					fclose(fd);
				} else { 
					fprintf(stderr,"Could not open %s\n",argv[xarg]);
					cmderr = TRUE;
				}
			} else {
				size = utf16size(argv[xarg]);
				if (size) {
					filename = (char*)malloc(2*size + 2);
					if (filename) {
						makeutf16(filename,argv[xarg]);
#if POSIXACLS
						if (local_build_mapping(context.mapping,filename)) {
							printf("*** Could not get user mapping data\n");
							warnings++;
						}
#endif
						if (opt_b)
							cmderr = backup(filename);
						else {
							if (opt_r)
								cmderr = listfiles(filename);
							else
								cmderr = singleshow(filename);
						}
#if POSIXACLS
						ntfs_free_mapping(context.mapping);
#endif
						free(filename);
					} else {
						fprintf(stderr,"No more memory\n");
						cmderr = TRUE;
					}
				} else {
					fprintf(stderr,"Bad UTF-8 name \"%s\"\n",argv[xarg]);
					cmderr = TRUE;
				}
			}
			break;
		case 2 :
			mode = 0;
			p = argv[xarg];
#if POSIXACLS
			pxdesc = encode_posix_acl(p);
			if (pxdesc) {
				size = utf16size(argv[xarg + 1]);
				if (size) {
					filename = (char*)malloc(2*size + 2);
					if (filename) {
						makeutf16(filename,argv[xarg + 1]);
						if (local_build_mapping(context.mapping,filename)) {
							printf("*** Could not get user mapping data\n");
							warnings++;
						}
						if (opt_r) {
							for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
								securdata[i] = (struct SECURITY_DATA*)NULL;
							recurseset_posix(filename,pxdesc);
						} else
							singleset_posix(filename,pxdesc);
						ntfs_free_mapping(context.mapping);
						free(filename);
					} else {
						fprintf(stderr,"No more memory\n");
						cmderr = TRUE;
					}
					chkfree(pxdesc,__FILE__,__LINE__);
				} else {
					fprintf(stderr,"Bad UTF-8 name \"%s\"\n",argv[xarg + 1]);
					cmderr = TRUE;
				}
			}
#else
			while ((*p >= '0') && (*p <= '7'))
				mode = (mode << 3) + (*p++) - '0';
			if (*p) {
				fprintf(stderr,"New mode should be given in octal\n");
				cmderr = TRUE;
			} else {
				size = utf16size(argv[xarg + 1]);
				if (size) {
					filename = (char*)malloc(2*size + 2);
					if (filename) {
						makeutf16(filename,argv[xarg + 1]);
#if POSIXACLS
						if (local_build_mapping(&context,filename)) {
							printf("*** Could not get user mapping data\n");
							warnings++;
						}
#endif
						if (opt_r) {
							for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
								securdata[i] = (struct SECURITY_DATA*)NULL;
							recurseset(filename,mode);
						} else
							singleset(filename,mode);
						free(filename);
					} else {
						fprintf(stderr,"No more memory\n");
						cmderr = TRUE;
					}
				} else {
					fprintf(stderr,"Bad UTF-8 name \"%s\"\n",argv[xarg + 1]);
					cmderr = TRUE;
				}
			}
#endif
			break;
#if FORCEMASK
		case 3 :
			mode = 0;
			forcemsk = 0;
			p = argv[xarg];
			while (*p) {
				if ((*p >= '0') && (*p <= '9'))
					forcemsk = (forcemsk << 4) + *p - '0';
				else forcemsk = (forcemsk << 4) + (*p & 7) + 9;
			p++;
			}
			p = argv[xarg + 1];
			while ((*p >= '0') && (*p <= '7'))
				mode = (mode << 3) + (*p++) - '0';
			if (*p) {
				fprintf(stderr,"New mode should be given in octal\n");
				cmderr = TRUE;
			} else {
				if (opt_r) {
					recurseset(argv[xarg + 2],mode);
				}
				else singleset(argv[xarg + 2],mode);
			}
			break;
#endif
		}
		if (warnings)
			printf("** %u %s signalled\n",warnings,
				(warnings > 1 ? "warnings were" : "warning was"));
		if (errors)
			printf("** %u %s found\n",errors,
				(errors > 1 ? "errors were" : "error was"));
		else
			printf("No errors were found\n");
		if (!isatty(1)) {
			fflush(stdout);
			if (warnings)
				fprintf(stderr,"** %u %s signalled\n",warnings,
					(warnings > 1 ? "warnings were" : "warning was"));
			if (errors)
				fprintf(stderr,"** %u %s found\n",errors,
					(errors > 1 ? "errors were" : "error was"));
			else
				fprintf(stderr,"No errors were found\n");
		freeblocks();
		}
	}
	dumpalloc("termination");
	if (cmderr || errors)
		exit(1);
	return (0);
}

#else

/*
 *		 Linux version
 */

int main(int argc, char *argv[])
{
	FILE *fd;
	unsigned int mode;
	const char *p;
	int xarg;
	BOOL cmderr;
	int i;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif

	printf("%s\n",BANNER);
	cmderr = FALSE;
	errors = 0;
	warnings = 0;
	xarg = getoptions(argc,argv);
	if (xarg) {
		for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
			securdata[i] = (struct SECURITY_DATA*)NULL;
#if POSIXACLS
		context.mapping[MAPUSERS] = (struct MAPPING*)NULL;
		context.mapping[MAPGROUPS] = (struct MAPPING*)NULL;
#endif
		firstalloc = (struct CHKALLOC*)NULL;
		mappingtype = MAPNONE;
		switch (argc - xarg) {
		case 0 :
			if (opt_h)
				showhex(stdin);
#if SELFTESTS & !USESTUBS
			if (opt_t)
				selftests();
#endif
			break;
		case 1 :
			if (opt_a)
				cmderr = audit(argv[xarg]);
			else
				if (opt_h) {
					fd = fopen(argv[xarg],"rb");
					if (fd) {
						showhex(fd);
						fclose(fd);
					} else { 
						fprintf(stderr,"Could not open %s\n",argv[xarg]);
						cmderr = TRUE;
					}
				} else
					if (opt_b)
						cmderr = backup(argv[xarg],"/");
					else
						if (opt_r)
							cmderr = listfiles(argv[xarg],"/");
						else
							if (opt_s)
								cmderr = dorestore(argv[xarg],stdin);
							else
								showmounted(argv[xarg]);
			break;
		case 2 :
			if (opt_b)
				cmderr = backup(argv[xarg],argv[xarg+1]);
			else
				if (opt_s) {
					fd = fopen(argv[xarg+1],"rb");
					if (fd) {
						if (dorestore(argv[xarg],fd))
							cmderr = TRUE;
						fclose(fd);
					} else { 
						fprintf(stderr,"Could not open %s\n",argv[xarg]);
						cmderr = TRUE;
					}
				} else
					cmderr = listfiles(argv[xarg],argv[xarg+1]);
			break;
		case 3 :
			mode = 0;
			p = argv[xarg+1];
#if POSIXACLS
			pxdesc = encode_posix_acl(p);
			if (pxdesc) {
				if (!getuid() && open_security_api()) {
					if (open_volume(argv[xarg],MS_NONE)) {
						if (opt_r) {
							for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
								securdata[i] = (struct SECURITY_DATA*)NULL;
							recurseset_posix(argv[xarg + 2],pxdesc);
						} else
							singleset_posix(argv[xarg + 2],pxdesc);
						close_volume(argv[xarg]);
					} else {
						fprintf(stderr,"Could not open volume %s\n",argv[xarg]);
						printerror(stderr);
						cmderr = TRUE;
					}
					close_security_api();
				} else {
					if (getuid())
						fprintf(stderr,"This is only possible as root\n");
					else
						fprintf(stderr,"Could not open security API\n");
					cmderr = TRUE;
				}
			chkfree(pxdesc,__FILE__,__LINE__);
			} else
				cmderr = TRUE;
#else
			while ((*p >= '0') && (*p <= '7'))
				mode = (mode << 3) + (*p++) - '0';
			if (*p) {
				fprintf(stderr,"New mode should be given in octal\n");
				cmderr = TRUE;
			} else
				if (!getuid() && open_security_api()) {
					if (open_volume(argv[xarg],MS_NONE)) {
						if (opt_r) {
							for (i=0; i<(MAXSECURID + (1 << SECBLKSZ) - 1)/(1 << SECBLKSZ); i++)
								securdata[i] = (struct SECURITY_DATA*)NULL;
							recurseset(argv[xarg + 2],mode);
						} else
							singleset(argv[xarg + 2],mode);
						close_volume(argv[xarg]);
					} else {
						fprintf(stderr,"Could not open volume %s\n",argv[xarg]);
						printerror(stderr);
						cmderr = TRUE;
					}
					close_security_api();
				} else {
					if (getuid())
						fprintf(stderr,"This is only possible as root\n");
					else
						fprintf(stderr,"Could not open security API\n");
					cmderr = TRUE;
				}
#endif
			break;
		}
		if (warnings)
			printf("** %u %s signalled\n",warnings,
				(warnings > 1 ? "warnings were" : "warning was"));
		if (errors)
			printf("** %u %s found\n",errors,
				(errors > 1 ? "errors were" : "error was"));
		else
			if (!cmderr)
				printf("No errors were found\n");
		if (!isatty(1)) {
			fflush(stdout);
			if (warnings)
				fprintf(stderr,"** %u %s signalled\n",warnings,
					(warnings > 1 ? "warnings were" : "warning was"));
			if (errors)
				fprintf(stderr,"** %u %s found\n",errors,
					(errors > 1 ? "errors were" : "error was"));
			else
				if (!cmderr)
					fprintf(stderr,"No errors were found\n");
		}
		freeblocks();
	} else
		cmderr = TRUE;
	dumpalloc("termination");
	if (cmderr || errors)
		exit(1);
	return (0);
}

#endif
