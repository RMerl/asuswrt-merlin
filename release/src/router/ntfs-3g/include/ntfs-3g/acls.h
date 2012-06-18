/*
 *
 * Copyright (c) 2007-2008 Jean-Pierre Andre
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

#ifndef ACLS_H
#define ACLS_H

/*
 *	JPA configuration modes for security.c / acls.c
 *	should be moved to some config file
 */

#define BUFSZ 1024		/* buffer size to read mapping file */
#define MAPPINGFILE ".NTFS-3G/UserMapping" /* default mapping file */
#define LINESZ 120              /* maximum useful size of a mapping line */
#define CACHE_PERMISSIONS_BITS 6  /* log2 of unitary allocation of permissions */
#define CACHE_PERMISSIONS_SIZE 262144 /* max cacheable permissions */

/*
 *	JPA The following must be in some library...
 *	but did not found out where
 */

#define endian_rev16(x) (((x >> 8) & 255) | ((x & 255) << 8))
#define endian_rev32(x) (((x >> 24) & 255) | ((x >> 8) & 0xff00) \
		| ((x & 0xff00) << 8) | ((x & 255) << 24))

#define cpu_to_be16(x) endian_rev16(cpu_to_le16(x))
#define cpu_to_be32(x) endian_rev32(cpu_to_le32(x))

/*
 *		Macro definitions needed to share code with secaudit
 */

#define NTFS_FIND_USID(map,uid,buf) ntfs_find_usid(map,uid,buf)
#define NTFS_FIND_GSID(map,gid,buf) ntfs_find_gsid(map,gid,buf)
#define NTFS_FIND_USER(map,usid) ntfs_find_user(map,usid)
#define NTFS_FIND_GROUP(map,gsid) ntfs_find_group(map,gsid)


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

/*
 *		A type large enough to hold any SID
 */

typedef char BIGSID[40];

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

typedef int (*FILEREADER)(void *fileid, char *buf, size_t size, off_t pos);

/*
 *		Constants defined in acls.c
 */

extern const SID *adminsid;
extern const SID *worldsid;

/*
 *		Functions defined in acls.c
 */

BOOL ntfs_valid_descr(const char *securattr, unsigned int attrsz);
BOOL ntfs_valid_pattern(const SID *sid);
BOOL ntfs_valid_sid(const SID *sid);
BOOL ntfs_same_sid(const SID *first, const SID *second);

BOOL ntfs_is_user_sid(const SID *usid);


int ntfs_sid_size(const SID * sid);
unsigned int ntfs_attr_size(const char *attr);

const SID *ntfs_find_usid(const struct MAPPING *usermapping,
			uid_t uid, SID *pdefsid);
const SID *ntfs_find_gsid(const struct MAPPING *groupmapping,
			gid_t gid, SID *pdefsid);
uid_t ntfs_find_user(const struct MAPPING *usermapping, const SID *usid);
gid_t ntfs_find_group(const struct MAPPING *groupmapping, const SID * gsid);
const SID *ntfs_acl_owner(const char *secattr);

#if POSIXACLS

BOOL ntfs_valid_posix(const struct POSIX_SECURITY *pxdesc);
void ntfs_sort_posix(struct POSIX_SECURITY *pxdesc);
int ntfs_merge_mode_posix(struct POSIX_SECURITY *pxdesc, mode_t mode);
struct POSIX_SECURITY *ntfs_build_inherited_posix(
		const struct POSIX_SECURITY *pxdesc, mode_t mode,
		mode_t umask, BOOL isdir);
struct POSIX_SECURITY *ntfs_replace_acl(const struct POSIX_SECURITY *oldpxdesc,
		const struct POSIX_ACL *newacl, int count, BOOL deflt);
struct POSIX_SECURITY *ntfs_build_permissions_posix(
			struct MAPPING* const mapping[],
			const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir);
struct POSIX_SECURITY *ntfs_merge_descr_posix(const struct POSIX_SECURITY *first,
			const struct POSIX_SECURITY *second);
char *ntfs_build_descr_posix(struct MAPPING* const mapping[],
			struct POSIX_SECURITY *pxdesc,
			int isdir, const SID *usid, const SID *gsid);

#endif /* POSIXACLS */

int ntfs_inherit_acl(const ACL *oldacl, ACL *newacl,
			const SID *usid, const SID *gsid, BOOL fordir);
int ntfs_build_permissions(const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir);
char *ntfs_build_descr(mode_t mode,
			int isdir, const SID * usid, const SID * gsid);
struct MAPLIST *ntfs_read_mapping(FILEREADER reader, void *fileid);
struct MAPPING *ntfs_do_user_mapping(struct MAPLIST *firstitem);
struct MAPPING *ntfs_do_group_mapping(struct MAPLIST *firstitem);
void ntfs_free_mapping(struct MAPPING *mapping[]);

#endif /* ACLS_H */

