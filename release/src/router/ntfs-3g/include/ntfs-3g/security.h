/*
 * security.h - Exports for handling security/ACLs in NTFS.  
 *              Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2004      Anton Altaparmakov
 * Copyright (c) 2005-2006 Szabolcs Szakacsits
 * Copyright (c) 2007-2008 Jean-Pierre Andre
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_SECURITY_H
#define _NTFS_SECURITY_H

#include "types.h"
#include "layout.h"
#include "inode.h"
#include "dir.h"

#ifndef POSIXACLS
#define POSIXACLS 0
#endif

typedef u16 be16;
typedef u32 be32;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define const_cpu_to_be16(x) ((((x) & 255L) << 8) + (((x) >> 8) & 255L))
#define const_cpu_to_be32(x) ((((x) & 255L) << 24) + (((x) & 0xff00L) << 8) \
			+ (((x) >> 8) & 0xff00L) + (((x) >> 24) & 255L))
#else
#define const_cpu_to_be16(x) (x)
#define const_cpu_to_be32(x) (x)
#endif

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
 *		Entry in the permissions cache
 *	Note : this cache is not organized as a generic cache
 */

struct CACHED_PERMISSIONS {
	uid_t uid;
	gid_t gid;
	le32 inh_fileid;
	le32 inh_dirid;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
	unsigned int pxdescsize:16;
#endif
	unsigned int mode:12;
	unsigned int valid:1;
} ;

/*
 *	Entry in the permissions cache for directories with no security_id
 */

struct CACHED_PERMISSIONS_LEGACY {
	struct CACHED_PERMISSIONS_LEGACY *next;
	struct CACHED_PERMISSIONS_LEGACY *previous;
	void *variable;
	size_t varsize;
		/* above fields must match "struct CACHED_GENERIC" */
	u64 mft_no;
	struct CACHED_PERMISSIONS perm;
} ;

/*
 *	Entry in the securid cache
 */

struct CACHED_SECURID {
	struct CACHED_SECURID *next;
	struct CACHED_SECURID *previous;
	void *variable;
	size_t varsize;
		/* above fields must match "struct CACHED_GENERIC" */
	uid_t uid;
	gid_t gid;
	unsigned int dmode;
	le32 securid;
} ;

/*
 *	Header of the security cache
 *	(has no cache structure by itself)
 */

struct CACHED_PERMISSIONS_HEADER {
	unsigned int last;
			/* statistics for permissions */
	unsigned long p_writes;
	unsigned long p_reads;
	unsigned long p_hits;
} ;

/*
 *	The whole permissions cache
 */

struct PERMISSIONS_CACHE {
	struct CACHED_PERMISSIONS_HEADER head;
	struct CACHED_PERMISSIONS *cachetable[1]; /* array of variable size */
} ;

/*
 *	Security flags values
 */

enum {
	SECURITY_DEFAULT,	/* rely on fuse for permissions checking */
	SECURITY_RAW,		/* force same ownership/permissions on files */
	SECURITY_ADDSECURIDS,	/* upgrade old security descriptors */
	SECURITY_STATICGRPS,	/* use static groups for access control */
	SECURITY_WANTED		/* a security related option was present */
} ;

/*
 *	Security context, needed by most security functions
 */

enum { MAPUSERS, MAPGROUPS, MAPCOUNT } ;

struct SECURITY_CONTEXT {
	ntfs_volume *vol;
	struct MAPPING *mapping[MAPCOUNT];
	struct PERMISSIONS_CACHE **pseccache;
	uid_t uid; /* uid of user requesting (not the mounter) */
	gid_t gid; /* gid of user requesting (not the mounter) */
	pid_t tid; /* thread id of thread requesting */
	mode_t umask; /* umask of requesting thread */
	} ;

#if POSIXACLS

/*
 *		       Posix ACL structures
 */
  
struct POSIX_ACE {
	u16 tag;
	u16 perms;
	s32 id;   
} __attribute__((__packed__));
        
struct POSIX_ACL {
	u8 version;
	u8 flags;
	u16 filler;
	struct POSIX_ACE ace[0];
} __attribute__((__packed__));

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

#define POSIX_ACL_EXTENSIONS (POSIX_ACL_USER | POSIX_ACL_GROUP | POSIX_ACL_MASK)
        
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

#endif

extern BOOL ntfs_guid_is_zero(const GUID *guid);
extern char *ntfs_guid_to_mbs(const GUID *guid, char *guid_str);

/**
 * ntfs_sid_is_valid - determine if a SID is valid
 * @sid:	SID for which to determine if it is valid
 *
 * Determine if the SID pointed to by @sid is valid.
 *
 * Return TRUE if it is valid and FALSE otherwise.
 */
static __inline__ BOOL ntfs_sid_is_valid(const SID *sid)
{
	if (!sid || sid->revision != SID_REVISION ||
			sid->sub_authority_count > SID_MAX_SUB_AUTHORITIES)
		return FALSE;
	return TRUE;
}

extern int ntfs_sid_to_mbs_size(const SID *sid);
extern char *ntfs_sid_to_mbs(const SID *sid, char *sid_str,
		size_t sid_str_size);
extern void ntfs_generate_guid(GUID *guid);
extern int ntfs_sd_add_everyone(ntfs_inode *ni);

extern le32 ntfs_security_hash(const SECURITY_DESCRIPTOR_RELATIVE *sd, 
			       const u32 len);

int ntfs_build_mapping(struct SECURITY_CONTEXT *scx, const char *usermap_path,
		BOOL allowdef);
int ntfs_get_owner_mode(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, struct stat*);
int ntfs_set_mode(struct SECURITY_CONTEXT *scx, ntfs_inode *ni, mode_t mode);
BOOL ntfs_allowed_as_owner(struct SECURITY_CONTEXT *scx, ntfs_inode *ni);
int ntfs_allowed_access(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, int accesstype);
BOOL old_ntfs_allowed_dir_access(struct SECURITY_CONTEXT *scx,
		const char *path, int accesstype);

#if POSIXACLS
le32 ntfs_alloc_securid(struct SECURITY_CONTEXT *scx,
		uid_t uid, gid_t gid, ntfs_inode *dir_ni,
		mode_t mode, BOOL isdir);
#else
le32 ntfs_alloc_securid(struct SECURITY_CONTEXT *scx,
		uid_t uid, gid_t gid, mode_t mode, BOOL isdir);
#endif
int ntfs_set_owner(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
		uid_t uid, gid_t gid);
int ntfs_set_ownmod(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid, mode_t mode);
#if POSIXACLS
int ntfs_set_owner_mode(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid,
		mode_t mode, struct POSIX_SECURITY *pxdesc);
#else
int ntfs_set_owner_mode(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid, mode_t mode);
#endif
le32 ntfs_inherited_id(struct SECURITY_CONTEXT *scx,
		ntfs_inode *dir_ni, BOOL fordir);
int ntfs_open_secure(ntfs_volume *vol);
void ntfs_close_secure(struct SECURITY_CONTEXT *scx);

#if POSIXACLS

int ntfs_set_inherited_posix(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid,
		ntfs_inode *dir_ni, mode_t mode);
int ntfs_get_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name, char *value, size_t size);
int ntfs_set_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name, const char *value, size_t size,
			int flags);
int ntfs_remove_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name);
#endif

int ntfs_get_ntfs_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			char *value, size_t size);
int ntfs_set_ntfs_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *value, size_t size, int flags); 

int ntfs_get_ntfs_attrib(ntfs_inode *ni, char *value, size_t size);
int ntfs_set_ntfs_attrib(ntfs_inode *ni,
			const char *value, size_t size,	int flags);
			

/*
 *		Security API for direct access to security descriptors
 *	based on Win32 API
 */

#define MAGIC_API 0x09042009

struct SECURITY_API {
	u32 magic;
	struct SECURITY_CONTEXT security;
	struct PERMISSIONS_CACHE *seccache;
} ;

/*
 *  The following constants are used in interfacing external programs.
 *  They are not to be stored on disk and must be defined in their
 *  native cpu representation.
 *  When disk representation (le) is needed, use SE_DACL_PRESENT, etc.
 */
enum {	OWNER_SECURITY_INFORMATION = 1,
	GROUP_SECURITY_INFORMATION = 2,
	DACL_SECURITY_INFORMATION = 4,
	SACL_SECURITY_INFORMATION = 8
} ;

int ntfs_get_file_security(struct SECURITY_API *scapi,
                const char *path, u32 selection,  
                char *buf, u32 buflen, u32 *psize);
int ntfs_set_file_security(struct SECURITY_API *scapi,
		const char *path, u32 selection, const char *attr);
int ntfs_get_file_attributes(struct SECURITY_API *scapi,
		const char *path);
BOOL ntfs_set_file_attributes(struct SECURITY_API *scapi,
		const char *path, s32 attrib);
BOOL ntfs_read_directory(struct SECURITY_API *scapi,
		const char *path, ntfs_filldir_t callback, void *context);
int ntfs_read_sds(struct SECURITY_API *scapi,
		char *buf, u32 size, u32 offset);
INDEX_ENTRY *ntfs_read_sii(struct SECURITY_API *scapi,
		INDEX_ENTRY *entry);
INDEX_ENTRY *ntfs_read_sdh(struct SECURITY_API *scapi,
		INDEX_ENTRY *entry);
struct SECURITY_API *ntfs_initialize_file_security(const char *device,
                                int flags);
BOOL ntfs_leave_file_security(struct SECURITY_API *scx);

int ntfs_get_usid(struct SECURITY_API *scapi, uid_t uid, char *buf);
int ntfs_get_gsid(struct SECURITY_API *scapi, gid_t gid, char *buf);
int ntfs_get_user(struct SECURITY_API *scapi, const SID *usid);
int ntfs_get_group(struct SECURITY_API *scapi, const SID *gsid);

#endif /* defined _NTFS_SECURITY_H */
