/**
 * acls.c - General function to process NTFS ACLs
 *
 *	This module is part of ntfs-3g library, but may also be
 *	integrated in tools running over Linux or Windows
 *
 * Copyright (c) 2007-2009 Jean-Pierre Andre
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

#ifdef HAVE_CONFIG_H
				/*
				 * integration into ntfs-3g
				 */
#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "types.h"
#include "layout.h"
#include "security.h"
#include "acls.h"
#include "misc.h"
#else

				/*
				 * integration into secaudit, check whether Win32,
				 * may have to be adapted to compiler or something else
				 */

#ifndef WIN32
#if defined(__WIN32) | defined(__WIN32__) | defined(WNSC)
#define WIN32 1
#endif
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>

				/*
				 * integration into secaudit/Win32
				 */
#ifdef WIN32
#include <fcntl.h>
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
#include <endian.h>
#include <unistd.h>
#include <dlfcn.h>
#endif /* STSC */
#endif /* WIN32 */
#include "secaudit.h"
#endif /* HAVE_CONFIG_H */

/*
 *	A few useful constants
 */

/*
 *		null SID (S-1-0-0)
 */

static const char nullsidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 0,	/* base */
		0, 0, 0, 0 	/* 1st level */
	};

static const SID *nullsid = (const SID*)nullsidbytes;

/*
 *		SID for world  (S-1-1-0)
 */

static const char worldsidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 1,	/* base */
		0, 0, 0, 0	/* 1st level */
} ;

const SID *worldsid = (const SID*)worldsidbytes;

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

const SID *adminsid = (const SID*)adminsidbytes;

/*
 *		SID for system
 */

static const char systemsidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 5,	/* base */
		18, 0, 0, 0 	/* 1st level */
	};

static const SID *systemsid = (const SID*)systemsidbytes;

/*
 *		SID for generic creator-owner
 *		S-1-3-0
 */

static const char ownersidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 3,	/* base */
		0, 0, 0, 0	/* 1st level */
} ;

static const SID *ownersid = (const SID*)ownersidbytes;

/*
 *		SID for generic creator-group
 *		S-1-3-1
 */

static const char groupsidbytes[] = {
		1,		/* revision */
		1,		/* auth count */
		0, 0, 0, 0, 0, 3,	/* base */
		1, 0, 0, 0	/* 1st level */
} ;

static const SID *groupsid = (const SID*)groupsidbytes;

/*
 *		Determine the size of a SID
 */

int ntfs_sid_size(const SID * sid)
{
	return (sid->sub_authority_count * 4 + 8);
}

/*
 *		Test whether two SID are equal
 */

BOOL ntfs_same_sid(const SID *first, const SID *second)
{
	int size;

	size = ntfs_sid_size(first);
	return ((ntfs_sid_size(second) == size)
		&& !memcmp(first, second, size));
}

/*
 *		Test whether a SID means "world user"
 *	Local users group also recognized as world
 */

static int is_world_sid(const SID * usid)
{
	return (
	     /* check whether S-1-1-0 : world */
	       ((usid->sub_authority_count == 1)
	    && (usid->identifier_authority.high_part ==  const_cpu_to_be16(0))
	    && (usid->identifier_authority.low_part ==  const_cpu_to_be32(1))
	    && (usid->sub_authority[0] == const_cpu_to_le32(0)))

	     /* check whether S-1-5-32-545 : local user */
	  ||   ((usid->sub_authority_count == 2)
	    && (usid->identifier_authority.high_part ==  const_cpu_to_be16(0))
	    && (usid->identifier_authority.low_part ==  const_cpu_to_be32(5))
	    && (usid->sub_authority[0] == const_cpu_to_le32(32))
	    && (usid->sub_authority[1] == const_cpu_to_le32(545)))
		);
}

/*
 *		Test whether a SID means "some user (or group)"
 *	Currently we only check for S-1-5-21... but we should
 *	probably test for other configurations
 */

BOOL ntfs_is_user_sid(const SID *usid)
{
	return ((usid->sub_authority_count == 5)
	    && (usid->identifier_authority.high_part ==  const_cpu_to_be16(0))
	    && (usid->identifier_authority.low_part ==  const_cpu_to_be32(5))
	    && (usid->sub_authority[0] ==  const_cpu_to_le32(21)));
}

/*
 *		Determine the size of a security attribute
 *	whatever the order of fields
 */

unsigned int ntfs_attr_size(const char *attr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pdacl;
	const ACL *psacl;
	const SID *psid;
	unsigned int offdacl;
	unsigned int offsacl;
	unsigned int offowner;
	unsigned int offgroup;
	unsigned int endsid;
	unsigned int endacl;
	unsigned int attrsz;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
		/*
		 * First check group, which is the last field in all descriptors
		 * we build, and in most descriptors built by Windows
		 */
	attrsz = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
	offgroup = le32_to_cpu(phead->group);
	if (offgroup >= attrsz) {
			/* find end of GSID */
		psid = (const SID*)&attr[offgroup];
		endsid = offgroup + ntfs_sid_size(psid);
		if (endsid > attrsz) attrsz = endsid;
	}
	offowner = le32_to_cpu(phead->owner);
	if (offowner >= attrsz) {
			/* find end of USID */
		psid = (const SID*)&attr[offowner];
		endsid = offowner + ntfs_sid_size(psid);
		attrsz = endsid;
	}
	offsacl = le32_to_cpu(phead->sacl);
	if (offsacl >= attrsz) {
			/* find end of SACL */
		psacl = (const ACL*)&attr[offsacl];
		endacl = offsacl + le16_to_cpu(psacl->size);
		if (endacl > attrsz)
			attrsz = endacl;
	}


		/* find end of DACL */
	offdacl = le32_to_cpu(phead->dacl);
	if (offdacl >= attrsz) {
		pdacl = (const ACL*)&attr[offdacl];
		endacl = offdacl + le16_to_cpu(pdacl->size);
		if (endacl > attrsz)
			attrsz = endacl;
	}
	return (attrsz);
}

/*
 *		Do sanity checks on a SID read from storage
 *	(just check revision and number of authorities)
 */

BOOL ntfs_valid_sid(const SID *sid)
{
	return ((sid->revision == SID_REVISION)
		&& (sid->sub_authority_count >= 1)
		&& (sid->sub_authority_count <= 8));
}

/*
 *		Check whether a SID is acceptable for an implicit
 *	mapping pattern.
 *	It should have been already checked it is a valid user SID.
 *
 *	The last authority reference has to be >= 1000 (Windows usage)
 *	and <= 0x7fffffff, so that 30 bits from a uid and 30 more bits
 *      from a gid an be inserted with no overflow.
 */

BOOL ntfs_valid_pattern(const SID *sid)
{
	int cnt;
	u32 auth;
	le32 leauth;

	cnt = sid->sub_authority_count;
	leauth = sid->sub_authority[cnt-1];
	auth = le32_to_cpu(leauth);
	return ((auth >= 1000) && (auth <= 0x7fffffff));
}

/*
 *		Compute the uid or gid associated to a SID
 *	through an implicit mapping
 *
 *	Returns 0 (root) if it does not match pattern
 */

static u32 findimplicit(const SID *xsid, const SID *pattern, int parity)
{
	BIGSID defsid;
	SID *psid;
	u32 xid; /* uid or gid */
	int cnt;
	u32 carry;
	le32 leauth;
	u32 uauth;
	u32 xlast;
	u32 rlast;

	memcpy(&defsid,pattern,ntfs_sid_size(pattern));
	psid = (SID*)&defsid;
	cnt = psid->sub_authority_count;
	xid = 0;
	if (xsid->sub_authority_count == cnt) {
		psid->sub_authority[cnt-1] = xsid->sub_authority[cnt-1];
		leauth = xsid->sub_authority[cnt-1];
		xlast = le32_to_cpu(leauth);
		leauth = pattern->sub_authority[cnt-1];
		rlast = le32_to_cpu(leauth);

		if ((xlast > rlast) && !((xlast ^ rlast ^ parity) & 1)) {
			/* direct check for basic situation */
			if (ntfs_same_sid(psid,xsid))
				xid = ((xlast - rlast) >> 1) & 0x3fffffff;
			else {
				/*
				 * check whether part of mapping had to be
				 * recorded in a higher level authority
			 	 */
				carry = 1;
				do {
					leauth = psid->sub_authority[cnt-2];
					uauth = le32_to_cpu(leauth) + 1;
					psid->sub_authority[cnt-2]
						= cpu_to_le32(uauth);
				} while (!ntfs_same_sid(psid,xsid)
					 && (++carry < 4));
				if (carry < 4)
					xid = (((xlast - rlast) >> 1)
						& 0x3fffffff) | (carry << 30);
			}
		}
	}
	return (xid);
}

/*
 *		Find usid mapped to a Linux user
 *	Returns NULL if not found
 */

const SID *ntfs_find_usid(const struct MAPPING* usermapping,
		uid_t uid, SID *defusid)
{
	const struct MAPPING *p;
	const SID *sid;
	le32 leauth;
	u32 uauth;
	int cnt;

	if (!uid)
		sid = adminsid;
	else {
		p = usermapping;
		while (p && p->xid && ((uid_t)p->xid != uid))
			p = p->next;
		if (p && !p->xid) {
			/*
			 * default pattern has been reached :
			 * build an implicit SID according to pattern
			 * (the pattern format was checked while reading
			 * the mapping file)
			 */
			memcpy(defusid, p->sid, ntfs_sid_size(p->sid));
			cnt = defusid->sub_authority_count;
			leauth = defusid->sub_authority[cnt-1];
			uauth = le32_to_cpu(leauth) + 2*(uid & 0x3fffffff);
			defusid->sub_authority[cnt-1] = cpu_to_le32(uauth);
			if (uid & 0xc0000000) {
				leauth = defusid->sub_authority[cnt-2];
				uauth = le32_to_cpu(leauth) + ((uid >> 30) & 3);
				defusid->sub_authority[cnt-2] = cpu_to_le32(uauth);
			}
			sid = defusid;
		} else
			sid = (p ? p->sid : (const SID*)NULL);
	}
	return (sid);
}

/*
 *		Find Linux group mapped to a gsid
 *	Returns 0 (root) if not found
 */

const SID *ntfs_find_gsid(const struct MAPPING* groupmapping,
		gid_t gid, SID *defgsid)
{
	const struct MAPPING *p;
	const SID *sid;
	le32 leauth;
	u32 uauth;
	int cnt;

	if (!gid)
		sid = adminsid;
	else {
		p = groupmapping;
		while (p && p->xid && ((gid_t)p->xid != gid))
			p = p->next;
		if (p && !p->xid) {
			/*
			 * default pattern has been reached :
			 * build an implicit SID according to pattern
			 * (the pattern format was checked while reading
			 * the mapping file)
			 */
			memcpy(defgsid, p->sid, ntfs_sid_size(p->sid));
			cnt = defgsid->sub_authority_count;
			leauth = defgsid->sub_authority[cnt-1];
			uauth = le32_to_cpu(leauth) + 2*(gid & 0x3fffffff) + 1;
			defgsid->sub_authority[cnt-1] = cpu_to_le32(uauth);
			if (gid & 0xc0000000) {
				leauth = defgsid->sub_authority[cnt-2];
				uauth = le32_to_cpu(leauth) + ((gid >> 30) & 3);
				defgsid->sub_authority[cnt-2] = cpu_to_le32(uauth);
			}
			sid = defgsid;
		} else
			sid = (p ? p->sid : (const SID*)NULL);
	}
	return (sid);
}

/*
 *		Find Linux owner mapped to a usid
 *	Returns 0 (root) if not found
 */

uid_t ntfs_find_user(const struct MAPPING* usermapping, const SID *usid)
{
	uid_t uid;
	const struct MAPPING *p;

	p = usermapping;
	while (p && p->xid && !ntfs_same_sid(usid, p->sid))
		p = p->next;
	if (p && !p->xid)
		/*
		 * No explicit mapping found, try implicit mapping
		 */
		uid = findimplicit(usid,p->sid,0);
	else
		uid = (p ? p->xid : 0);
	return (uid);
}

/*
 *		Find Linux group mapped to a gsid
 *	Returns 0 (root) if not found
 */

gid_t ntfs_find_group(const struct MAPPING* groupmapping, const SID * gsid)
{
	gid_t gid;
	const struct MAPPING *p;
	int gsidsz;

	gsidsz = ntfs_sid_size(gsid);
	p = groupmapping;
	while (p && p->xid && !ntfs_same_sid(gsid, p->sid))
		p = p->next;
	if (p && !p->xid)
		/*
		 * No explicit mapping found, try implicit mapping
		 */
		gid = findimplicit(gsid,p->sid,1);
	else
		gid = (p ? p->xid : 0);
	return (gid);
}

/*
 *		Check the validity of the ACEs in a DACL or SACL
 */

static BOOL valid_acl(const ACL *pacl, unsigned int end)
{
	const ACCESS_ALLOWED_ACE *pace;
	unsigned int offace;
	unsigned int acecnt;
	unsigned int acesz;
	unsigned int nace;
	BOOL ok;

	ok = TRUE;
	acecnt = le16_to_cpu(pacl->ace_count);
	offace = sizeof(ACL);
	for (nace = 0; (nace < acecnt) && ok; nace++) {
		/* be sure the beginning is within range */
		if ((offace + sizeof(ACCESS_ALLOWED_ACE)) > end)
			ok = FALSE;
		else {
			pace = (const ACCESS_ALLOWED_ACE*)
				&((const char*)pacl)[offace];
			acesz = le16_to_cpu(pace->size);
			if (((offace + acesz) > end)
			   || !ntfs_valid_sid(&pace->sid)
			   || ((ntfs_sid_size(&pace->sid) + 8) != (int)acesz))
				 ok = FALSE;
			offace += acesz;
		}
	}
	return (ok);
}

/*
 *		Do sanity checks on security descriptors read from storage
 *	basically, we make sure that every field holds within
 *	allocated storage
 *	Should not be called with a NULL argument
 *	returns TRUE if considered safe
 *		if not, error should be logged by caller
 */

BOOL ntfs_valid_descr(const char *securattr, unsigned int attrsz)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pdacl;
	const ACL *psacl;
	unsigned int offdacl;
	unsigned int offsacl;
	unsigned int offowner;
	unsigned int offgroup;
	BOOL ok;

	ok = TRUE;

	/*
	 * first check overall size if within allocation range
	 * and a DACL is present
	 * and owner and group SID are valid
	 */

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	offsacl = le32_to_cpu(phead->sacl);
	offowner = le32_to_cpu(phead->owner);
	offgroup = le32_to_cpu(phead->group);
	pdacl = (const ACL*)&securattr[offdacl];
	psacl = (const ACL*)&securattr[offsacl];

		/*
		 * size check occurs before the above pointers are used
		 *
		 * "DR Watson" standard directory on WinXP has an
		 * old revision and no DACL though SE_DACL_PRESENT is set
		 */
	if ((attrsz >= sizeof(SECURITY_DESCRIPTOR_RELATIVE))
		&& (phead->revision == SECURITY_DESCRIPTOR_REVISION)
		&& (offowner >= sizeof(SECURITY_DESCRIPTOR_RELATIVE))
		&& ((offowner + 2) < attrsz)
		&& (offgroup >= sizeof(SECURITY_DESCRIPTOR_RELATIVE))
		&& ((offgroup + 2) < attrsz)
		&& (!offdacl
			|| ((offdacl >= sizeof(SECURITY_DESCRIPTOR_RELATIVE))
			    && (offdacl+sizeof(ACL) < attrsz)))
		&& (!offsacl
			|| ((offsacl >= sizeof(SECURITY_DESCRIPTOR_RELATIVE))
			    && (offsacl+sizeof(ACL) < attrsz)))
		&& !(phead->owner & const_cpu_to_le32(3))
		&& !(phead->group & const_cpu_to_le32(3))
		&& !(phead->dacl & const_cpu_to_le32(3))
		&& !(phead->sacl & const_cpu_to_le32(3))
		&& (ntfs_attr_size(securattr) <= attrsz)
		&& ntfs_valid_sid((const SID*)&securattr[offowner])
		&& ntfs_valid_sid((const SID*)&securattr[offgroup])
			/*
			 * if there is an ACL, as indicated by offdacl,
			 * require SE_DACL_PRESENT
			 * but "Dr Watson" has SE_DACL_PRESENT though no DACL
			 */
		&& (!offdacl
		    || ((phead->control & SE_DACL_PRESENT)
			&& ((pdacl->revision == ACL_REVISION)
			   || (pdacl->revision == ACL_REVISION_DS))))
			/* same for SACL */
		&& (!offsacl
		    || ((phead->control & SE_SACL_PRESENT)
			&& ((psacl->revision == ACL_REVISION)
			    || (psacl->revision == ACL_REVISION_DS))))) {
			/*
			 *  Check the DACL and SACL if present
			 */
		if ((offdacl && !valid_acl(pdacl,attrsz - offdacl))
		   || (offsacl && !valid_acl(psacl,attrsz - offsacl)))
			ok = FALSE;
	} else
		ok = FALSE;
	return (ok);
}

/*
 *		Copy the inheritable parts of an ACL
 *
 *	Returns the size of the new ACL
 *		or zero if nothing is inheritable
 */

int ntfs_inherit_acl(const ACL *oldacl, ACL *newacl,
			const SID *usid, const SID *gsid, BOOL fordir)
{
	unsigned int src;
	unsigned int dst;
	int oldcnt;
	int newcnt;
	unsigned int selection;
	int nace;
	int acesz;
	int usidsz;
	int gsidsz;
	const ACCESS_ALLOWED_ACE *poldace;
	ACCESS_ALLOWED_ACE *pnewace;

	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);

	/* ACL header */

	newacl->revision = ACL_REVISION;
	newacl->alignment1 = 0;
	newacl->alignment2 = const_cpu_to_le16(0);
	src = dst = sizeof(ACL);

	selection = (fordir ? CONTAINER_INHERIT_ACE : OBJECT_INHERIT_ACE);
	newcnt = 0;
	oldcnt = le16_to_cpu(oldacl->ace_count);
	for (nace = 0; nace < oldcnt; nace++) {
		poldace = (const ACCESS_ALLOWED_ACE*)((const char*)oldacl + src);
		acesz = le16_to_cpu(poldace->size);
			/* inheritance for access */
		if (poldace->flags & selection) {
			pnewace = (ACCESS_ALLOWED_ACE*)
					((char*)newacl + dst);
			memcpy(pnewace,poldace,acesz);
				/*
				 * Replace generic creator-owner and
				 * creator-group by owner and group
				 */
			if (ntfs_same_sid(&pnewace->sid, ownersid)) {
				memcpy(&pnewace->sid, usid, usidsz);
				acesz = usidsz + 8;
				pnewace->size = cpu_to_le16(acesz);
			}
			if (ntfs_same_sid(&pnewace->sid, groupsid)) {
				memcpy(&pnewace->sid, gsid, gsidsz);
				acesz = gsidsz + 8;
				pnewace->size = cpu_to_le16(acesz);
			}
			if (pnewace->mask & GENERIC_ALL) {
				pnewace->mask &= ~GENERIC_ALL;
				if (fordir)
					pnewace->mask |= OWNER_RIGHTS
							| DIR_READ
							| DIR_WRITE
							| DIR_EXEC;
				else
			/*
			 * The last flag is not defined for a file,
			 * however Windows sets it, so do the same
			 */
					pnewace->mask |= OWNER_RIGHTS
							| FILE_READ
							| FILE_WRITE
							| FILE_EXEC
							| cpu_to_le32(0x40);
			}
				/* remove inheritance flags */
			pnewace->flags &= ~(OBJECT_INHERIT_ACE
						| CONTAINER_INHERIT_ACE
						| INHERIT_ONLY_ACE);
			dst += acesz;
			newcnt++;
		}
			/* inheritance for further inheritance */
		if (fordir
		   && (poldace->flags
			   & (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE))) {
			pnewace = (ACCESS_ALLOWED_ACE*)
					((char*)newacl + dst);
			memcpy(pnewace,poldace,acesz);
				/*
				 * Replace generic creator-owner and
				 * creator-group by owner and group
				 */
			if (ntfs_same_sid(&pnewace->sid, ownersid)) {
				memcpy(&pnewace->sid, usid, usidsz);
				acesz = usidsz + 8;
			}
			if (ntfs_same_sid(&pnewace->sid, groupsid)) {
				memcpy(&pnewace->sid, gsid, gsidsz);
				acesz = gsidsz + 8;
			}
			dst += acesz;
			newcnt++;
		}
		src += acesz;
	}
		/*
		 * Adjust header if something was inherited
		 */
	if (dst > sizeof(ACL)) {
		newacl->ace_count = cpu_to_le16(newcnt);
		newacl->size = cpu_to_le16(dst);
	} else
		dst = 0;
	return (dst);
}

#if POSIXACLS

/*
 *		Do sanity checks on a Posix descriptor
 *	Should not be called with a NULL argument
 *	returns TRUE if considered safe
 *		if not, error should be logged by caller
 */

BOOL ntfs_valid_posix(const struct POSIX_SECURITY *pxdesc)
{
	const struct POSIX_ACL *pacl;
	int i;
	BOOL ok;
	u16 tag;
	u32 id;
	int perms;
	struct {
		u16 previous;
		u32 previousid;
		u16 tagsset;
		mode_t mode;
		int owners;
		int groups;
		int others;
	} checks[2], *pchk;

	for (i=0; i<2; i++) {
		checks[i].mode = 0;
		checks[i].tagsset = 0;
		checks[i].owners = 0;
		checks[i].groups = 0;
		checks[i].others = 0;
		checks[i].previous = 0;
		checks[i].previousid = 0;
	}
	ok = TRUE;
	pacl = &pxdesc->acl;
			/*
			 * header (strict for now)
			 */
	if ((pacl->version != POSIX_VERSION)
	    || (pacl->flags != 0)
	    || (pacl->filler != 0))
		ok = FALSE;
			/*
			 * Reject multiple owner, group or other
			 * but do not require them to be present
			 * Also check the ACEs are in correct order
			 * which implies there is no duplicates
			 */
	for (i=0; i<pxdesc->acccnt + pxdesc->defcnt; i++) {
		if (i >= pxdesc->firstdef)
			pchk = &checks[1];
		else
			pchk = &checks[0];
		perms = pacl->ace[i].perms;
		tag = pacl->ace[i].tag;
		pchk->tagsset |= tag;
		id = pacl->ace[i].id;
		if (perms & ~7) ok = FALSE;
		if ((tag < pchk->previous)
			|| ((tag == pchk->previous)
			 && (id <= pchk->previousid)))
				ok = FALSE;
		pchk->previous = tag;
		pchk->previousid = id;
		switch (tag) {
		case POSIX_ACL_USER_OBJ :
			if (pchk->owners++)
				ok = FALSE;
			if (id != (u32)-1)
				ok = FALSE;
			pchk->mode |= perms << 6;
			break;
		case POSIX_ACL_GROUP_OBJ :
			if (pchk->groups++)
				ok = FALSE;
			if (id != (u32)-1)
				ok = FALSE;
			pchk->mode = (pchk->mode & 07707) | (perms << 3);
			break;
		case POSIX_ACL_OTHER :
			if (pchk->others++)
				ok = FALSE;
			if (id != (u32)-1)
				ok = FALSE;
			pchk->mode |= perms;
			break;
		case POSIX_ACL_USER :
		case POSIX_ACL_GROUP :
			if (id == (u32)-1)
				ok = FALSE;
			break;
		case POSIX_ACL_MASK :
			if (id != (u32)-1)
				ok = FALSE;
			pchk->mode = (pchk->mode & 07707) | (perms << 3);
			break;
		default :
			ok = FALSE;
			break;
		}
	}
	if ((pxdesc->acccnt > 0)
	   && ((checks[0].owners != 1) || (checks[0].groups != 1) 
		|| (checks[0].others != 1)))
		ok = FALSE;
		/* do not check owner, group or other are present in */
		/* the default ACL, Windows does not necessarily set them */
			/* descriptor */
	if (pxdesc->defcnt && (pxdesc->acccnt > pxdesc->firstdef))
		ok = FALSE;
	if ((pxdesc->acccnt < 0) || (pxdesc->defcnt < 0))
		ok = FALSE;
			/* check mode, unless null or no tag set */
	if (pxdesc->mode
	    && checks[0].tagsset
	    && (checks[0].mode != (pxdesc->mode & 0777)))
		ok = FALSE;
			/* check tagsset */
	if (pxdesc->tagsset != checks[0].tagsset)
		ok = FALSE;
	return (ok);
}

/*
 *		Set standard header data into a Posix ACL
 *	The mode argument should provide the 3 upper bits of target mode
 */

static mode_t posix_header(struct POSIX_SECURITY *pxdesc, mode_t basemode)
{
	mode_t mode;
	u16 tagsset;
	struct POSIX_ACE *pace;
	int i;

	mode = basemode & 07000;
	tagsset = 0;
	for (i=0; i<pxdesc->acccnt; i++) {
		pace = &pxdesc->acl.ace[i];
		tagsset |= pace->tag;
		switch(pace->tag) {
		case POSIX_ACL_USER_OBJ :
			mode |= (pace->perms & 7) << 6;
			break;
		case POSIX_ACL_GROUP_OBJ :
		case POSIX_ACL_MASK :
			mode = (mode & 07707) | ((pace->perms & 7) << 3);
			break;
		case POSIX_ACL_OTHER :
			mode |= pace->perms & 7;
			break;
		default :
			break;
		}
	}
	pxdesc->tagsset = tagsset;
	pxdesc->mode = mode;
	pxdesc->acl.version = POSIX_VERSION;
	pxdesc->acl.flags = 0;
	pxdesc->acl.filler = 0;
	return (mode);
}

/*
 *		Sort ACEs in a Posix ACL
 *	This is useful for always getting reusable converted ACLs,
 *	it also helps in merging ACEs.
 *	Repeated tag+id are allowed and not merged here.
 *
 *	Tags should be in ascending sequence and for a repeatable tag
 *	ids should be in ascending sequence.
 */

void ntfs_sort_posix(struct POSIX_SECURITY *pxdesc)
{
	struct POSIX_ACL *pacl;
	struct POSIX_ACE ace;
	int i;
	int offs;
	BOOL done;
	u16 tag;
	u16 previous;
	u32 id;
	u32 previousid;


			/*
			 * Check sequencing of tag+id in access ACE's
			 */
	pacl = &pxdesc->acl;
	do {
		done = TRUE;
		previous = pacl->ace[0].tag;
		previousid = pacl->ace[0].id;
		for (i=1; i<pxdesc->acccnt; i++) {
			tag = pacl->ace[i].tag;
			id = pacl->ace[i].id;

			if ((tag < previous)
			   || ((tag == previous) && (id < previousid))) {
				done = FALSE;
				memcpy(&ace,&pacl->ace[i-1],sizeof(struct POSIX_ACE));
				memcpy(&pacl->ace[i-1],&pacl->ace[i],sizeof(struct POSIX_ACE));
				memcpy(&pacl->ace[i],&ace,sizeof(struct POSIX_ACE));
			} else {
				previous = tag;
				previousid = id;
			}
		}
	} while (!done);
				/*
				 * Same for default ACEs
				 */
	do {
		done = TRUE;
		if ((pxdesc->defcnt) > 1) {
			offs = pxdesc->firstdef;
			previous = pacl->ace[offs].tag;
			previousid = pacl->ace[offs].id;
			for (i=offs+1; i<offs+pxdesc->defcnt; i++) {
				tag = pacl->ace[i].tag;
				id = pacl->ace[i].id;

				if ((tag < previous)
				   || ((tag == previous) && (id < previousid))) {
					done = FALSE;
					memcpy(&ace,&pacl->ace[i-1],sizeof(struct POSIX_ACE));
					memcpy(&pacl->ace[i-1],&pacl->ace[i],sizeof(struct POSIX_ACE));
					memcpy(&pacl->ace[i],&ace,sizeof(struct POSIX_ACE));
				} else {
					previous = tag;
					previousid = id;
				}
			}
		}
	} while (!done);
}

/*
 *		Merge a new mode into a Posix descriptor
 *	The Posix descriptor is not reallocated, its size is unchanged
 *
 *	returns 0 if ok
 */

int ntfs_merge_mode_posix(struct POSIX_SECURITY *pxdesc, mode_t mode)
{
	int i;
	BOOL maskfound;
	struct POSIX_ACE *pace;
	int todo;

	maskfound = FALSE;
	todo = POSIX_ACL_USER_OBJ | POSIX_ACL_GROUP_OBJ | POSIX_ACL_OTHER;
	for (i=pxdesc->acccnt-1; i>=0; i--) {
		pace = &pxdesc->acl.ace[i];
		switch(pace->tag) {
		case POSIX_ACL_USER_OBJ :
			pace->perms = (mode >> 6) & 7;
			todo &= ~POSIX_ACL_USER_OBJ;
			break;
		case POSIX_ACL_GROUP_OBJ :
			if (!maskfound)
				pace->perms = (mode >> 3) & 7;
			todo &= ~POSIX_ACL_GROUP_OBJ;
			break;
		case POSIX_ACL_MASK :
			pace->perms = (mode >> 3) & 7;
			maskfound = TRUE;
			break;
		case POSIX_ACL_OTHER :
			pace->perms = mode & 7;
			todo &= ~POSIX_ACL_OTHER;
			break;
		default :
			break;
		}
	}
	pxdesc->mode = mode;
	return (todo ? -1 : 0);
}

/*
 *		Replace an access or default Posix ACL
 *	The resulting ACL is checked for validity
 *
 *	Returns a new ACL or NULL if there is a problem
 */

struct POSIX_SECURITY *ntfs_replace_acl(const struct POSIX_SECURITY *oldpxdesc,
		const struct POSIX_ACL *newacl, int count, BOOL deflt)
{
	struct POSIX_SECURITY *newpxdesc;
	size_t newsize;
	int offset;
	int oldoffset;
	int i;

	if (deflt)
		newsize = sizeof(struct POSIX_SECURITY)
			+ (oldpxdesc->acccnt + count)*sizeof(struct POSIX_ACE);
	else
		newsize = sizeof(struct POSIX_SECURITY)
			+ (oldpxdesc->defcnt + count)*sizeof(struct POSIX_ACE);
	newpxdesc = (struct POSIX_SECURITY*)malloc(newsize);
	if (newpxdesc) {
		if (deflt) {
			offset = oldpxdesc->acccnt;
			newpxdesc->acccnt = oldpxdesc->acccnt;
			newpxdesc->defcnt = count;
			newpxdesc->firstdef = offset;
					/* copy access ACEs */
			for (i=0; i<newpxdesc->acccnt; i++)
				newpxdesc->acl.ace[i] = oldpxdesc->acl.ace[i];
					/* copy default ACEs */
			for (i=0; i<count; i++)
				newpxdesc->acl.ace[i + offset] = newacl->ace[i];
		} else {
			offset = count;
			newpxdesc->acccnt = count;
			newpxdesc->defcnt = oldpxdesc->defcnt;
			newpxdesc->firstdef = count;
					/* copy access ACEs */
			for (i=0; i<count; i++)
				newpxdesc->acl.ace[i] = newacl->ace[i];
					/* copy default ACEs */
			oldoffset = oldpxdesc->firstdef;
			for (i=0; i<newpxdesc->defcnt; i++)
				newpxdesc->acl.ace[i + offset] = oldpxdesc->acl.ace[i + oldoffset];
		}
			/* assume special flags unchanged */
		posix_header(newpxdesc, oldpxdesc->mode);
		if (!ntfs_valid_posix(newpxdesc)) {
			/* do not log, this is an application error */
			free(newpxdesc);
			newpxdesc = (struct POSIX_SECURITY*)NULL;
			errno = EINVAL;
		}
	} else
		errno = ENOMEM;
	return (newpxdesc);
}

/*
 *		Build an inherited Posix descriptor from parent
 *	descriptor (if any) restricted to creation mode
 *
 *	Returns the inherited descriptor or NULL if there is a problem
 */

struct POSIX_SECURITY *ntfs_build_inherited_posix(
		const struct POSIX_SECURITY *pxdesc, mode_t mode,
		mode_t mask, BOOL isdir)
{
	struct POSIX_SECURITY *pydesc;
	struct POSIX_ACE *pyace;
	int count;
	int defcnt;
	int size;
	int i;
	s16 tagsset;

	if (pxdesc && pxdesc->defcnt) {
		if (isdir)
			count = 2*pxdesc->defcnt + 3;
		else
			count = pxdesc->defcnt + 3;
	} else
		count = 3;
	pydesc = (struct POSIX_SECURITY*)malloc(
		sizeof(struct POSIX_SECURITY) + count*sizeof(struct POSIX_ACE));
	if (pydesc) {
			/*
			 * Copy inherited tags and adapt perms
			 * Use requested mode, ignoring umask
			 * (not possible with older versions of fuse)
			 */
		tagsset = 0;
		defcnt = (pxdesc ? pxdesc->defcnt : 0);
		for (i=defcnt-1; i>=0; i--) {
			pyace = &pydesc->acl.ace[i];
			*pyace = pxdesc->acl.ace[pxdesc->firstdef + i];
			switch (pyace->tag) {
			case POSIX_ACL_USER_OBJ :
				pyace->perms &= (mode >> 6) & 7;
				break;
			case POSIX_ACL_GROUP_OBJ :
				if (!(tagsset & POSIX_ACL_MASK))
					pyace->perms &= (mode >> 3) & 7;
				break;
			case POSIX_ACL_OTHER :
				pyace->perms &= mode & 7;
				break;
			case POSIX_ACL_MASK :
				pyace->perms &= (mode >> 3) & 7;
				break;
			default :
				break;
			}
			tagsset |= pyace->tag;
		}
		pydesc->acccnt = defcnt;
		/*
		 * If some standard tags were missing, append them from mode
		 * and sort the list
		 * Here we have to use the umask'ed mode
		 */
		if (~tagsset & (POSIX_ACL_USER_OBJ
				 | POSIX_ACL_GROUP_OBJ | POSIX_ACL_OTHER)) {
			i = defcnt;
				/* owner was missing */
			if (!(tagsset & POSIX_ACL_USER_OBJ)) {
				pyace = &pydesc->acl.ace[i];
				pyace->tag = POSIX_ACL_USER_OBJ;
				pyace->id = -1;
				pyace->perms = ((mode & ~mask) >> 6) & 7;
				tagsset |= POSIX_ACL_USER_OBJ;
				i++;
			}
				/* owning group was missing */
			if (!(tagsset & POSIX_ACL_GROUP_OBJ)) {
				pyace = &pydesc->acl.ace[i];
				pyace->tag = POSIX_ACL_GROUP_OBJ;
				pyace->id = -1;
				pyace->perms = ((mode & ~mask) >> 3) & 7;
				tagsset |= POSIX_ACL_GROUP_OBJ;
				i++;
			}
				/* other was missing */
			if (!(tagsset & POSIX_ACL_OTHER)) {
				pyace = &pydesc->acl.ace[i];
				pyace->tag = POSIX_ACL_OTHER;
				pyace->id = -1;
				pyace->perms = mode & ~mask & 7;
				tagsset |= POSIX_ACL_OTHER;
				i++;
			}
			pydesc->acccnt = i;
			pydesc->firstdef = i;
			pydesc->defcnt = 0;
			ntfs_sort_posix(pydesc);
		}

		/*
		 * append as a default ACL if a directory
		 */
		pydesc->firstdef = pydesc->acccnt;
		if (defcnt && isdir) {
			size = sizeof(struct POSIX_ACE)*defcnt;
			memcpy(&pydesc->acl.ace[pydesc->firstdef],
				 &pxdesc->acl.ace[pxdesc->firstdef],size);
			pydesc->defcnt = defcnt;
		} else {
			pydesc->defcnt = 0;
		}
			/* assume special bits are not inherited */
		posix_header(pydesc, mode & 07000);
		if (!ntfs_valid_posix(pydesc)) {
			ntfs_log_error("Error building an inherited Posix desc\n");
			errno = EIO;
			free(pydesc);
			pydesc = (struct POSIX_SECURITY*)NULL;
		}
	} else
		errno = ENOMEM;
	return (pydesc);
}

static int merge_lists_posix(struct POSIX_ACE *targetace,
		const struct POSIX_ACE *firstace,
		const struct POSIX_ACE *secondace,
		int firstcnt, int secondcnt)
{
	int k;

	k = 0;
		/*
		 * No list is exhausted :
		 *    if same tag+id in both list :
		 *       ignore ACE from second list
		 *    else take the one with smaller tag+id
		 */
	while ((firstcnt > 0) && (secondcnt > 0))
		if ((firstace->tag == secondace->tag)
		    && (firstace->id == secondace->id)) {
			secondace++;
			secondcnt--;
		} else
			if ((firstace->tag < secondace->tag)
			   || ((firstace->tag == secondace->tag)
				&& (firstace->id < secondace->id))) {
				targetace->tag = firstace->tag;
				targetace->id = firstace->id;
				targetace->perms = firstace->perms;
				firstace++;
				targetace++;
				firstcnt--;
				k++;
			} else {
				targetace->tag = secondace->tag;
				targetace->id = secondace->id;
				targetace->perms = secondace->perms;
				secondace++;
				targetace++;
				secondcnt--;
				k++;
			}
		/*
		 * One list is exhausted, copy the other one
		 */
	while (firstcnt > 0) {
		targetace->tag = firstace->tag;
		targetace->id = firstace->id;
		targetace->perms = firstace->perms;
		firstace++;
		targetace++;
		firstcnt--;
		k++;
	}
	while (secondcnt > 0) {
		targetace->tag = secondace->tag;
		targetace->id = secondace->id;
		targetace->perms = secondace->perms;
		secondace++;
		targetace++;
		secondcnt--;
		k++;
	}
	return (k);
}

/*
 *		Merge two Posix ACLs
 *	The input ACLs have to be adequately sorted
 *
 *	Returns the merged ACL, which is allocated and has to be freed by caller,
 *	or NULL if failed
 */

struct POSIX_SECURITY *ntfs_merge_descr_posix(const struct POSIX_SECURITY *first,
			const struct POSIX_SECURITY *second)
{
	struct POSIX_SECURITY *pxdesc;
	struct POSIX_ACE *targetace;
	const struct POSIX_ACE *firstace;
	const struct POSIX_ACE *secondace;
	size_t size;
	int k;

	size = sizeof(struct POSIX_SECURITY)
		+ (first->acccnt + first->defcnt
			+ second->acccnt + second->defcnt)*sizeof(struct POSIX_ACE);
	pxdesc = (struct POSIX_SECURITY*)malloc(size);
	if (pxdesc) {
			/*
			 * merge access ACEs
			 */
		firstace = first->acl.ace;
		secondace = second->acl.ace;
		targetace = pxdesc->acl.ace;
		k = merge_lists_posix(targetace,firstace,secondace,
			first->acccnt,second->acccnt);
		pxdesc->acccnt = k;
			/*
			 * merge default ACEs
			 */
		pxdesc->firstdef = k;
		firstace = &first->acl.ace[first->firstdef];
		secondace = &second->acl.ace[second->firstdef];
		targetace = &pxdesc->acl.ace[k];
		k = merge_lists_posix(targetace,firstace,secondace,
			first->defcnt,second->defcnt);
		pxdesc->defcnt = k;
			/*
			 * build header
			 */
		pxdesc->acl.version = POSIX_VERSION;
		pxdesc->acl.flags = 0;
		pxdesc->acl.filler = 0;
		pxdesc->mode = 0;
		pxdesc->tagsset = 0;
	} else
		errno = ENOMEM;
	return (pxdesc);
}

struct BUILD_CONTEXT {
	BOOL isdir;
	BOOL adminowns;
	BOOL groupowns;
	u16 selfuserperms;
	u16 selfgrpperms;
	u16 grpperms;
	u16 othperms;
	u16 mask;
	u16 designates;
	u16 withmask;
	u16 rootspecial;
} ;



static BOOL build_user_denials(ACL *pacl,
		const SID *usid, struct MAPPING* const mapping[],
		ACE_FLAGS flags, const struct POSIX_ACE *pxace,
		struct BUILD_CONTEXT *pset)
{
	BIGSID defsid;
	ACCESS_ALLOWED_ACE *pdace;
	const SID *sid;
	int sidsz;
	int pos;
	int acecnt;
	le32 grants;
	le32 denials;
	u16 perms;
	u16 mixperms;
	u16 tag;
	BOOL rejected;
	BOOL rootuser;
	BOOL avoidmask;

	rejected = FALSE;
	tag = pxace->tag;
	perms = pxace->perms;
	rootuser = FALSE;
	pos = le16_to_cpu(pacl->size);
	acecnt = le16_to_cpu(pacl->ace_count);
	avoidmask = (pset->mask == (POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X))
			&& ((pset->designates && pset->withmask)
			   || (!pset->designates && !pset->withmask));
	if (tag == POSIX_ACL_USER_OBJ) {
		sid = usid;
		sidsz = ntfs_sid_size(sid);
		grants = OWNER_RIGHTS;
	} else {
		if (pxace->id) {
			sid = NTFS_FIND_USID(mapping[MAPUSERS],
				pxace->id, (SID*)&defsid);
			grants = WORLD_RIGHTS;
		} else {
			sid = adminsid;
			rootuser = TRUE;
			grants = WORLD_RIGHTS & ~ROOT_OWNER_UNMARK;
		}
		if (sid) {
			sidsz = ntfs_sid_size(sid);
			/*
			 * Insert denial of complement of mask for
			 * each designated user (except root)
			 * WRITE_OWNER is inserted so that
			 * the mask can be identified
			 */
			if (!avoidmask && !rootuser) {
				denials = WRITE_OWNER;
				pdace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
				if (pset->isdir) {
					if (!(pset->mask & POSIX_PERM_X))
						denials |= DIR_EXEC;
					if (!(pset->mask & POSIX_PERM_W))
						denials |= DIR_WRITE;
					if (!(pset->mask & POSIX_PERM_R))
						denials |= DIR_READ;
				} else {
					if (!(pset->mask & POSIX_PERM_X))
						denials |= FILE_EXEC;
					if (!(pset->mask & POSIX_PERM_W))
						denials |= FILE_WRITE;
					if (!(pset->mask & POSIX_PERM_R))
						denials |= FILE_READ;
				}
				if (rootuser)
					grants &= ~ROOT_OWNER_UNMARK;
				pdace->type = ACCESS_DENIED_ACE_TYPE;
				pdace->flags = flags;
				pdace->size = cpu_to_le16(sidsz + 8);
				pdace->mask = denials;
				memcpy((char*)&pdace->sid, sid, sidsz);
				pos += sidsz + 8;
				acecnt++;
			}
		} else
			rejected = TRUE;
	}
	if (!rejected) {
		if (pset->isdir) {
			if (perms & POSIX_PERM_X)
				grants |= DIR_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= DIR_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= DIR_READ;
		} else {
			if (perms & POSIX_PERM_X)
				grants |= FILE_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= FILE_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= FILE_READ;
		}

		/* a possible ACE to deny owner what he/she would */
		/* induely get from administrator, group or world */
		/* unless owner is administrator or group */

		denials = const_cpu_to_le32(0);
		pdace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
		if (!pset->adminowns && !rootuser) {
			if (!pset->groupowns) {
				mixperms = pset->grpperms | pset->othperms;
				if (tag == POSIX_ACL_USER_OBJ)
					mixperms |= pset->selfuserperms;
				if (pset->isdir) {
					if (mixperms & POSIX_PERM_X)
						denials |= DIR_EXEC;
					if (mixperms & POSIX_PERM_W)
						denials |= DIR_WRITE;
					if (mixperms & POSIX_PERM_R)
						denials |= DIR_READ;
				} else {
					if (mixperms & POSIX_PERM_X)
						denials |= FILE_EXEC;
					if (mixperms & POSIX_PERM_W)
						denials |= FILE_WRITE;
					if (mixperms & POSIX_PERM_R)
						denials |= FILE_READ;
				}
			} else {
				mixperms = ~pset->grpperms & pset->othperms;
				if (tag == POSIX_ACL_USER_OBJ)
					mixperms |= pset->selfuserperms;
				if (pset->isdir) {
					if (mixperms & POSIX_PERM_X)
						denials |= DIR_EXEC;
					if (mixperms & POSIX_PERM_W)
						denials |= DIR_WRITE;
					if (mixperms & POSIX_PERM_R)
						denials |= DIR_READ;
				} else {
					if (mixperms & POSIX_PERM_X)
						denials |= FILE_EXEC;
					if (mixperms & POSIX_PERM_W)
						denials |= FILE_WRITE;
					if (mixperms & POSIX_PERM_R)
						denials |= FILE_READ;
				}
			}
			denials &= ~grants;
			if (denials) {
				pdace->type = ACCESS_DENIED_ACE_TYPE;
				pdace->flags = flags;
				pdace->size = cpu_to_le16(sidsz + 8);
				pdace->mask = denials;
				memcpy((char*)&pdace->sid, sid, sidsz);
				pos += sidsz + 8;
				acecnt++;
			}
		}
	}
	pacl->size = cpu_to_le16(pos);
	pacl->ace_count = cpu_to_le16(acecnt);
	return (!rejected);
}

static BOOL build_user_grants(ACL *pacl,
		const SID *usid, struct MAPPING* const mapping[],
		ACE_FLAGS flags, const struct POSIX_ACE *pxace,
		struct BUILD_CONTEXT *pset)
{
	BIGSID defsid;
	ACCESS_ALLOWED_ACE *pgace;
	const SID *sid;
	int sidsz;
	int pos;
	int acecnt;
	le32 grants;
	u16 perms;
	u16 tag;
	BOOL rejected;
	BOOL rootuser;

	rejected = FALSE;
	tag = pxace->tag;
	perms = pxace->perms;
	rootuser = FALSE;
	pos = le16_to_cpu(pacl->size);
	acecnt = le16_to_cpu(pacl->ace_count);
	if (tag == POSIX_ACL_USER_OBJ) {
		sid = usid;
		sidsz = ntfs_sid_size(sid);
		grants = OWNER_RIGHTS;
	} else {
		if (pxace->id) {
			sid = NTFS_FIND_USID(mapping[MAPUSERS],
				pxace->id, (SID*)&defsid);
			if (sid)
				sidsz = ntfs_sid_size(sid);
			else
				rejected = TRUE;
			grants = WORLD_RIGHTS;
		} else {
			sid = adminsid;
			sidsz = ntfs_sid_size(sid);
			rootuser = TRUE;
			grants = WORLD_RIGHTS & ~ROOT_OWNER_UNMARK;
		}
	}
	if (!rejected) {
		if (pset->isdir) {
			if (perms & POSIX_PERM_X)
				grants |= DIR_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= DIR_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= DIR_READ;
		} else {
			if (perms & POSIX_PERM_X)
				grants |= FILE_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= FILE_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= FILE_READ;
		}
		if (rootuser)
			grants &= ~ROOT_OWNER_UNMARK;
		pgace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
		pgace->type = ACCESS_ALLOWED_ACE_TYPE;
		pgace->size = cpu_to_le16(sidsz + 8);
		pgace->flags = flags;
		pgace->mask = grants;
		memcpy((char*)&pgace->sid, sid, sidsz);
		pos += sidsz + 8;
		acecnt = le16_to_cpu(pacl->ace_count) + 1;
		pacl->ace_count = cpu_to_le16(acecnt);
		pacl->size = cpu_to_le16(pos);
	}
	return (!rejected);
}


			/* a grant ACE for group */
			/* unless group-obj has the same rights as world */
			/* but present if group is owner or owner is administrator */
			/* this ACE will be inserted after denials for group */

static BOOL build_group_denials_grant(ACL *pacl,
		const SID *gsid, struct MAPPING* const mapping[],
		ACE_FLAGS flags, const struct POSIX_ACE *pxace,
		struct BUILD_CONTEXT *pset)
{
	BIGSID defsid;
	ACCESS_ALLOWED_ACE *pdace;
	ACCESS_ALLOWED_ACE *pgace;
	const SID *sid;
	int sidsz;
	int pos;
	int acecnt;
	le32 grants;
	le32 denials;
	u16 perms;
	u16 mixperms;
	u16 tag;
	BOOL avoidmask;
	BOOL rootgroup;
	BOOL rejected;

	rejected = FALSE;
	tag = pxace->tag;
	perms = pxace->perms;
	pos = le16_to_cpu(pacl->size);
	acecnt = le16_to_cpu(pacl->ace_count);
	rootgroup = FALSE;
	avoidmask = (pset->mask == (POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X))
			&& ((pset->designates && pset->withmask)
			   || (!pset->designates && !pset->withmask));
	if (tag == POSIX_ACL_GROUP_OBJ)
		sid = gsid;
	else
		if (pxace->id)
			sid = NTFS_FIND_GSID(mapping[MAPGROUPS],
				pxace->id, (SID*)&defsid);
		else {
			sid = adminsid;
			rootgroup = TRUE;
		}
	if (sid) {
		sidsz = ntfs_sid_size(sid);
		/*
		 * Insert denial of complement of mask for
		 * each group
		 * WRITE_OWNER is inserted so that
		 * the mask can be identified
		 * Note : this mask may lead on Windows to
		 * deny rights to administrators belonging
		 * to some user group
		 */
		if ((!avoidmask && !rootgroup)
		    || (pset->rootspecial
			&& (tag == POSIX_ACL_GROUP_OBJ))) {
			denials = WRITE_OWNER;
			pdace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
			if (pset->isdir) {
				if (!(pset->mask & POSIX_PERM_X))
					denials |= DIR_EXEC;
				if (!(pset->mask & POSIX_PERM_W))
					denials |= DIR_WRITE;
				if (!(pset->mask & POSIX_PERM_R))
					denials |= DIR_READ;
			} else {
				if (!(pset->mask & POSIX_PERM_X))
					denials |= FILE_EXEC;
				if (!(pset->mask & POSIX_PERM_W))
					denials |= FILE_WRITE;
				if (!(pset->mask & POSIX_PERM_R))
					denials |= FILE_READ;
			}
			pdace->type = ACCESS_DENIED_ACE_TYPE;
			pdace->flags = flags;
			pdace->size = cpu_to_le16(sidsz + 8);
			pdace->mask = denials;
			memcpy((char*)&pdace->sid, sid, sidsz);
			pos += sidsz + 8;
			acecnt++;
		}
	} else
		rejected = TRUE;
	if (!rejected
	    && (pset->adminowns
		|| pset->groupowns
		|| avoidmask
		|| rootgroup
		|| (perms != pset->othperms))) {
		grants = WORLD_RIGHTS;
		if (rootgroup)
			grants &= ~ROOT_GROUP_UNMARK;
		if (pset->isdir) {
			if (perms & POSIX_PERM_X)
				grants |= DIR_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= DIR_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= DIR_READ;
		} else {
			if (perms & POSIX_PERM_X)
				grants |= FILE_EXEC;
			if (perms & POSIX_PERM_W)
				grants |= FILE_WRITE;
			if (perms & POSIX_PERM_R)
				grants |= FILE_READ;
		}

		/* a possible ACE to deny group what it would get from world */
		/* or administrator, unless owner is administrator or group */

		denials = const_cpu_to_le32(0);
		pdace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
		if (!pset->adminowns
		    && !pset->groupowns
		    && !rootgroup) {
			mixperms = pset->othperms;
			if (tag == POSIX_ACL_GROUP_OBJ)
				mixperms |= pset->selfgrpperms;
			if (pset->isdir) {
				if (mixperms & POSIX_PERM_X)
					denials |= DIR_EXEC;
				if (mixperms & POSIX_PERM_W)
					denials |= DIR_WRITE;
				if (mixperms & POSIX_PERM_R)
					denials |= DIR_READ;
			} else {
				if (mixperms & POSIX_PERM_X)
					denials |= FILE_EXEC;
				if (mixperms & POSIX_PERM_W)
					denials |= FILE_WRITE;
				if (mixperms & POSIX_PERM_R)
					denials |= FILE_READ;
			}
			denials &= ~(grants | OWNER_RIGHTS);
			if (denials) {
				pdace->type = ACCESS_DENIED_ACE_TYPE;
				pdace->flags = flags;
				pdace->size = cpu_to_le16(sidsz + 8);
				pdace->mask = denials;
				memcpy((char*)&pdace->sid, sid, sidsz);
				pos += sidsz + 8;
				acecnt++;
			}
		}

			/* now insert grants to group if more than world */
		if (pset->adminowns
			|| pset->groupowns
			|| (avoidmask && (pset->designates || pset->withmask))
			|| (perms & ~pset->othperms)
			|| (pset->rootspecial
			   && (tag == POSIX_ACL_GROUP_OBJ))
			|| (tag == POSIX_ACL_GROUP)) {
			if (rootgroup)
				grants &= ~ROOT_GROUP_UNMARK;
			pgace = (ACCESS_DENIED_ACE*)&((char*)pacl)[pos];
			pgace->type = ACCESS_ALLOWED_ACE_TYPE;
			pgace->flags = flags;
			pgace->size = cpu_to_le16(sidsz + 8);
			pgace->mask = grants;
			memcpy((char*)&pgace->sid, sid, sidsz);
			pos += sidsz + 8;
			acecnt++;
		}
	}
	pacl->size = cpu_to_le16(pos);
	pacl->ace_count = cpu_to_le16(acecnt);
	return (!rejected);
}


/*
 *		Build an ACL composed of several ACE's
 *	returns size of ACL or zero if failed
 *
 *	Three schemes are defined :
 *
 *	1) if root is neither owner nor group up to 7 ACE's are set up :
 *	- denials to owner (preventing grants to world or group to apply)
 *        + mask denials to designated user (unless mask allows all)
 *        + denials to designated user
 *	- grants to owner (always present - first grant)
 *        + grants to designated user
 *        + mask denial to group (unless mask allows all)
 *	- denials to group (preventing grants to world to apply) 
 *	- grants to group (unless group has no more than world rights)
 *        + mask denials to designated group (unless mask allows all)
 *        + grants to designated group
 *        + denials to designated group
 *	- grants to world (unless none)
 *	- full privileges to administrator, always present
 *	- full privileges to system, always present
 *
 *	The same scheme is applied for Posix ACLs, with the mask represented
 *	as denials prepended to grants for designated users and groups
 *
 *	This is inspired by an Internet Draft from Marius Aamodt Eriksen
 *	for mapping NFSv4 ACLs to Posix ACLs (draft-ietf-nfsv4-acl-mapping-00.txt)
 *	More recent versions of the draft (draft-ietf-nfsv4-acl-mapping-05.txt)
 *	are not followed, as they ignore the Posix mask and lead to
 *	loss of compatibility with Linux implementations on other fs.
 *
 *	Note that denials to group are located after grants to owner.
 *	This only occurs in the unfrequent situation where world
 *	has more rights than group and cannot be avoided if owner and other
 *	have some common right which is denied to group (eg for mode 745
 *	executing has to be denied to group, but not to owner or world).
 *	This rare situation is processed by Windows correctly, but
 *	Windows utilities may want to change the order, with a
 *	consequence of applying the group denials to the Windows owner.
 *	The interpretation on Linux is not affected by the order change.
 *
 *	2) if root is either owner or group, two problems arise :
 *	- granting full rights to administrator (as needed to transpose
 *	  to Windows rights bypassing granting to root) would imply
 *	  Linux permissions to always be seen as rwx, no matter the chmod
 *	- there is no different SID to separate an administrator owner
 *	  from an administrator group. Hence Linux permissions for owner
 *	  would always be similar to permissions to group.
 *
 *	as a work-around, up to 5 ACE's are set up if owner or group :
 *	- grants to owner, always present at first position
 *	- grants to group, always present
 *	- grants to world, unless none
 *	- full privileges to administrator, always present
 *	- full privileges to system, always present
 *
 *	On Windows, these ACE's are processed normally, though they
 *	are redundant (owner, group and administrator are the same,
 *	as a consequence any denials would damage administrator rights)
 *	but on Linux, privileges to administrator are ignored (they
 *	are not needed as root has always full privileges), and
 *	neither grants to group are applied to owner, nor grants to
 *	world are applied to owner or group.
 *
 *	3) finally a similar situation arises when group is owner (they
 *	 have the same SID), but is not root.
 *	 In this situation up to 6 ACE's are set up :
 *
 *	- denials to owner (preventing grants to world to apply)
 *	- grants to owner (always present)
 *	- grants to group (unless groups has same rights as world)
 *	- grants to world (unless none)
 *	- full privileges to administrator, always present
 *	- full privileges to system, always present
 *
 *	On Windows, these ACE's are processed normally, though they
 *	are redundant (as owner and group are the same), but this has
 *	no impact on administrator rights
 *
 *	Special flags (S_ISVTX, S_ISGID, S_ISUID) :
 *	an extra null ACE is inserted to hold these flags, using
 *	the same conventions as cygwin.
 *
 */

static int buildacls_posix(struct MAPPING* const mapping[],
		char *secattr, int offs, const struct POSIX_SECURITY *pxdesc,
		int isdir, const SID *usid, const SID *gsid)
{
        struct BUILD_CONTEXT aceset[2], *pset;
	BOOL adminowns;
	BOOL groupowns;
	ACL *pacl;
	ACCESS_ALLOWED_ACE *pgace;
	ACCESS_ALLOWED_ACE *pdace;
	const struct POSIX_ACE *pxace;
	BOOL ok;
	mode_t mode;
	u16 tag;
	u16 perms;
	ACE_FLAGS flags;
	int pos;
	int i;
	int k;
	BIGSID defsid;
	const SID *sid;
	int acecnt;
	int usidsz;
	int gsidsz;
	int wsidsz;
	int asidsz;
	int ssidsz;
	int nsidsz;
	le32 grants;

	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);
	wsidsz = ntfs_sid_size(worldsid);
	asidsz = ntfs_sid_size(adminsid);
	ssidsz = ntfs_sid_size(systemsid);
	mode = pxdesc->mode;
		/* adminowns and groupowns are used for both lists */
	adminowns = ntfs_same_sid(usid, adminsid)
		 || ntfs_same_sid(gsid, adminsid);
	groupowns = !adminowns && ntfs_same_sid(usid, gsid);

	ok = TRUE;

	/* ACL header */
	pacl = (ACL*)&secattr[offs];
	pacl->revision = ACL_REVISION;
	pacl->alignment1 = 0;
	pacl->size = cpu_to_le16(sizeof(ACL) + usidsz + 8);
	pacl->ace_count = const_cpu_to_le16(0);
	pacl->alignment2 = const_cpu_to_le16(0);

		/*
		 * Determine what is allowed to some group or world
		 * to prevent designated users or other groups to get
		 * rights from groups or world
		 * Do the same if owner and group appear as designated
		 * user or group
		 * Also get global mask
		 */
	for (k=0; k<2; k++) {
		pset = &aceset[k];
		pset->selfuserperms = 0;
		pset->selfgrpperms = 0;
		pset->grpperms = 0;
		pset->othperms = 0;
		pset->mask = (POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X);
		pset->designates = 0;
		pset->withmask = 0;
		pset->rootspecial = 0;
		pset->adminowns = adminowns;
		pset->groupowns = groupowns;
		pset->isdir = isdir;
	}

	for (i=pxdesc->acccnt+pxdesc->defcnt-1; i>=0; i--) {
		if (i >= pxdesc->acccnt) {
			pset = &aceset[1];
			pxace = &pxdesc->acl.ace[i + pxdesc->firstdef - pxdesc->acccnt];
		} else {
			pset = &aceset[0];
			pxace = &pxdesc->acl.ace[i];
		}
		switch (pxace->tag) {
		case POSIX_ACL_USER :
			pset->designates++;
			if (pxace->id) {
				sid = NTFS_FIND_USID(mapping[MAPUSERS],
					pxace->id, (SID*)&defsid);
				if (sid && ntfs_same_sid(sid,usid))
					pset->selfuserperms |= pxace->perms;
			} else
				/* root as designated user is processed apart */
				pset->rootspecial = TRUE;
			break;
		case POSIX_ACL_GROUP :
			pset->designates++;
			if (pxace->id) {
				sid = NTFS_FIND_GSID(mapping[MAPUSERS],
					pxace->id, (SID*)&defsid);
				if (sid && ntfs_same_sid(sid,gsid))
					pset->selfgrpperms |= pxace->perms;
			} else
				/* root as designated group is processed apart */
				pset->rootspecial = TRUE;
			/* fall through */
		case POSIX_ACL_GROUP_OBJ :
			pset->grpperms |= pxace->perms;
			break;
		case POSIX_ACL_OTHER :
			pset->othperms = pxace->perms;
			break;
		case POSIX_ACL_MASK :
			pset->withmask++;
			pset->mask = pxace->perms;
		default :
			break;
		}
	}

if (pxdesc->defcnt && (pxdesc->firstdef != pxdesc->acccnt)) {
ntfs_log_error("** error : access and default not consecutive\n");
return (0);
}
			/*
			 * First insert all denials for owner and each
			 * designated user (with mask if needed)
			 */

	pacl->ace_count = const_cpu_to_le16(0);
	pacl->size = const_cpu_to_le16(sizeof(ACL));
	for (i=0; (i<(pxdesc->acccnt + pxdesc->defcnt)) && ok; i++) {
		if (i >= pxdesc->acccnt) {
			flags = INHERIT_ONLY_ACE
				| OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
			pset = &aceset[1];
			pxace = &pxdesc->acl.ace[i + pxdesc->firstdef - pxdesc->acccnt];
		} else {
			if (pxdesc->defcnt)
				flags = NO_PROPAGATE_INHERIT_ACE;
			else
				flags = (isdir ? DIR_INHERITANCE
					   : FILE_INHERITANCE);
			pset = &aceset[0];
			pxace = &pxdesc->acl.ace[i];
		}
		tag = pxace->tag;
		perms = pxace->perms;
		switch (tag) {

			/* insert denial ACEs for each owner or allowed user */

		case POSIX_ACL_USER :
		case POSIX_ACL_USER_OBJ :

			ok = build_user_denials(pacl,
				usid, mapping, flags, pxace, pset);
			break;
		default :
			break;
		}
	}

		/*
		 * for directories, insert a world execution denial
		 * inherited to plain files.
		 * This is to prevent Windows from granting execution
		 * of files through inheritance from parent directory
		 */

	if (isdir && ok) {
		pos = le16_to_cpu(pacl->size);
		pdace = (ACCESS_DENIED_ACE*) &secattr[offs + pos];
		pdace->type = ACCESS_DENIED_ACE_TYPE;
		pdace->flags = INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
		pdace->size = cpu_to_le16(wsidsz + 8);
		pdace->mask = FILE_EXEC;
		memcpy((char*)&pdace->sid, worldsid, wsidsz);
		pos += wsidsz + 8;
		acecnt = le16_to_cpu(pacl->ace_count) + 1;
		pacl->ace_count = cpu_to_le16(acecnt);
		pacl->size = cpu_to_le16(pos);
	}

			/*
			 * now insert (if needed)
			 * - grants to owner and designated users
			 * - mask and denials for all groups
			 * - grants to other
			 */

	for (i=0; (i<(pxdesc->acccnt + pxdesc->defcnt)) && ok; i++) {
		if (i >= pxdesc->acccnt) {
			flags = INHERIT_ONLY_ACE
				| OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
			pset = &aceset[1];
			pxace = &pxdesc->acl.ace[i + pxdesc->firstdef - pxdesc->acccnt];
		} else {
			if (pxdesc->defcnt)
				flags = NO_PROPAGATE_INHERIT_ACE;
			else
				flags = (isdir ? DIR_INHERITANCE
					   : FILE_INHERITANCE);
			pset = &aceset[0];
			pxace = &pxdesc->acl.ace[i];
		}
		tag = pxace->tag;
		perms = pxace->perms;
		switch (tag) {

			/* ACE for each owner or allowed user */

		case POSIX_ACL_USER :
		case POSIX_ACL_USER_OBJ :
			ok = build_user_grants(pacl,usid,
					mapping,flags,pxace,pset);
			break;

		case POSIX_ACL_GROUP :
		case POSIX_ACL_GROUP_OBJ :

			/* denials and grants for groups */

			ok = build_group_denials_grant(pacl,gsid,
					mapping,flags,pxace,pset);
			break;

		case POSIX_ACL_OTHER :

			/* grants for other users */

			pos = le16_to_cpu(pacl->size);
			pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
			grants = WORLD_RIGHTS;
			if (isdir) {
				if (perms & POSIX_PERM_X)
					grants |= DIR_EXEC;
				if (perms & POSIX_PERM_W)
					grants |= DIR_WRITE;
				if (perms & POSIX_PERM_R)
					grants |= DIR_READ;
			} else {
				if (perms & POSIX_PERM_X)
					grants |= FILE_EXEC;
				if (perms & POSIX_PERM_W)
					grants |= FILE_WRITE;
				if (perms & POSIX_PERM_R)
					grants |= FILE_READ;
			}
			pgace->type = ACCESS_ALLOWED_ACE_TYPE;
			pgace->flags = flags;
			pgace->size = cpu_to_le16(wsidsz + 8);
			pgace->mask = grants;
			memcpy((char*)&pgace->sid, worldsid, wsidsz);
			pos += wsidsz + 8;
			acecnt = le16_to_cpu(pacl->ace_count) + 1;
			pacl->ace_count = cpu_to_le16(acecnt);
			pacl->size = cpu_to_le16(pos);
			break;
		}
	}

	if (!ok) {
		errno = EINVAL;
		pos = 0;
	} else {
		/* an ACE for administrators */
		/* always full access */

		pos = le16_to_cpu(pacl->size);
		acecnt = le16_to_cpu(pacl->ace_count);
		if (isdir)
			flags = OBJECT_INHERIT_ACE
				| CONTAINER_INHERIT_ACE;
		else
			flags = NO_PROPAGATE_INHERIT_ACE;
		pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
		pgace->type = ACCESS_ALLOWED_ACE_TYPE;
		pgace->flags = flags;
		pgace->size = cpu_to_le16(asidsz + 8);
		grants = OWNER_RIGHTS | FILE_READ | FILE_WRITE | FILE_EXEC;
		pgace->mask = grants;
		memcpy((char*)&pgace->sid, adminsid, asidsz);
		pos += asidsz + 8;
		acecnt++;

		/* an ACE for system (needed ?) */
		/* always full access */

		pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
		pgace->type = ACCESS_ALLOWED_ACE_TYPE;
		pgace->flags = flags;
		pgace->size = cpu_to_le16(ssidsz + 8);
		grants = OWNER_RIGHTS | FILE_READ | FILE_WRITE | FILE_EXEC;
		pgace->mask = grants;
		memcpy((char*)&pgace->sid, systemsid, ssidsz);
		pos += ssidsz + 8;
		acecnt++;

		/* a null ACE to hold special flags */
		/* using the same representation as cygwin */

		if (mode & (S_ISVTX | S_ISGID | S_ISUID)) {
			nsidsz = ntfs_sid_size(nullsid);
			pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
			pgace->type = ACCESS_ALLOWED_ACE_TYPE;
			pgace->flags = NO_PROPAGATE_INHERIT_ACE;
			pgace->size = cpu_to_le16(nsidsz + 8);
			grants = const_cpu_to_le32(0);
			if (mode & S_ISUID)
				grants |= FILE_APPEND_DATA;
			if (mode & S_ISGID)
				grants |= FILE_WRITE_DATA;
			if (mode & S_ISVTX)
				grants |= FILE_READ_DATA;
			pgace->mask = grants;
			memcpy((char*)&pgace->sid, nullsid, nsidsz);
			pos += nsidsz + 8;
			acecnt++;
		}

		/* fix ACL header */
		pacl->size = cpu_to_le16(pos);
		pacl->ace_count = cpu_to_le16(acecnt);
	}
	return (ok ? pos : 0);
}

#endif /* POSIXACLS */

static int buildacls(char *secattr, int offs, mode_t mode, int isdir,
	       const SID * usid, const SID * gsid)
{
	ACL *pacl;
	ACCESS_ALLOWED_ACE *pgace;
	ACCESS_ALLOWED_ACE *pdace;
	BOOL adminowns;
	BOOL groupowns;
	ACE_FLAGS gflags;
	int pos;
	int acecnt;
	int usidsz;
	int gsidsz;
	int wsidsz;
	int asidsz;
	int ssidsz;
	int nsidsz;
	le32 grants;
	le32 denials;

	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);
	wsidsz = ntfs_sid_size(worldsid);
	asidsz = ntfs_sid_size(adminsid);
	ssidsz = ntfs_sid_size(systemsid);
	adminowns = ntfs_same_sid(usid, adminsid)
	         || ntfs_same_sid(gsid, adminsid);
	groupowns = !adminowns && ntfs_same_sid(usid, gsid);

	/* ACL header */
	pacl = (ACL*)&secattr[offs];
	pacl->revision = ACL_REVISION;
	pacl->alignment1 = 0;
	pacl->size = cpu_to_le16(sizeof(ACL) + usidsz + 8);
	pacl->ace_count = const_cpu_to_le16(1);
	pacl->alignment2 = const_cpu_to_le16(0);
	pos = sizeof(ACL);
	acecnt = 0;

	/* compute a grant ACE for owner */
	/* this ACE will be inserted after denial for owner */

	grants = OWNER_RIGHTS;
	if (isdir) {
		gflags = DIR_INHERITANCE;
		if (mode & S_IXUSR)
			grants |= DIR_EXEC;
		if (mode & S_IWUSR)
			grants |= DIR_WRITE;
		if (mode & S_IRUSR)
			grants |= DIR_READ;
	} else {
		gflags = FILE_INHERITANCE;
		if (mode & S_IXUSR)
			grants |= FILE_EXEC;
		if (mode & S_IWUSR)
			grants |= FILE_WRITE;
		if (mode & S_IRUSR)
			grants |= FILE_READ;
	}

	/* a possible ACE to deny owner what he/she would */
	/* induely get from administrator, group or world */
        /* unless owner is administrator or group */

	denials = const_cpu_to_le32(0);
	pdace = (ACCESS_DENIED_ACE*) &secattr[offs + pos];
	if (!adminowns) {
		if (!groupowns) {
			if (isdir) {
				pdace->flags = DIR_INHERITANCE;
				if (mode & (S_IXGRP | S_IXOTH))
					denials |= DIR_EXEC;
				if (mode & (S_IWGRP | S_IWOTH))
					denials |= DIR_WRITE;
				if (mode & (S_IRGRP | S_IROTH))
					denials |= DIR_READ;
			} else {
				pdace->flags = FILE_INHERITANCE;
				if (mode & (S_IXGRP | S_IXOTH))
					denials |= FILE_EXEC;
				if (mode & (S_IWGRP | S_IWOTH))
					denials |= FILE_WRITE;
				if (mode & (S_IRGRP | S_IROTH))
					denials |= FILE_READ;
			}
		} else {
			if (isdir) {
				pdace->flags = DIR_INHERITANCE;
				if ((mode & S_IXOTH) && !(mode & S_IXGRP))
					denials |= DIR_EXEC;
				if ((mode & S_IWOTH) && !(mode & S_IWGRP))
					denials |= DIR_WRITE;
				if ((mode & S_IROTH) && !(mode & S_IRGRP))
					denials |= DIR_READ;
			} else {
				pdace->flags = FILE_INHERITANCE;
				if ((mode & S_IXOTH) && !(mode & S_IXGRP))
					denials |= FILE_EXEC;
				if ((mode & S_IWOTH) && !(mode & S_IWGRP))
					denials |= FILE_WRITE;
				if ((mode & S_IROTH) && !(mode & S_IRGRP))
					denials |= FILE_READ;
			}
		}
		denials &= ~grants;
		if (denials) {
			pdace->type = ACCESS_DENIED_ACE_TYPE;
			pdace->size = cpu_to_le16(usidsz + 8);
			pdace->mask = denials;
			memcpy((char*)&pdace->sid, usid, usidsz);
			pos += usidsz + 8;
			acecnt++;
		}
	}
		/*
		 * for directories, a world execution denial
		 * inherited to plain files
		 */

	if (isdir) {
		pdace = (ACCESS_DENIED_ACE*) &secattr[offs + pos];
			pdace->type = ACCESS_DENIED_ACE_TYPE;
			pdace->flags = INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
			pdace->size = cpu_to_le16(wsidsz + 8);
			pdace->mask = FILE_EXEC;
			memcpy((char*)&pdace->sid, worldsid, wsidsz);
			pos += wsidsz + 8;
			acecnt++;
	}


		/* now insert grants to owner */
	pgace = (ACCESS_ALLOWED_ACE*) &secattr[offs + pos];
	pgace->type = ACCESS_ALLOWED_ACE_TYPE;
	pgace->size = cpu_to_le16(usidsz + 8);
	pgace->flags = gflags;
	pgace->mask = grants;
	memcpy((char*)&pgace->sid, usid, usidsz);
	pos += usidsz + 8;
	acecnt++;

	/* a grant ACE for group */
	/* unless group has the same rights as world */
	/* but present if group is owner or owner is administrator */
	/* this ACE will be inserted after denials for group */

	if (adminowns
	    || groupowns
	    || (((mode >> 3) ^ mode) & 7)) {
		grants = WORLD_RIGHTS;
		if (isdir) {
			gflags = DIR_INHERITANCE;
			if (mode & S_IXGRP)
				grants |= DIR_EXEC;
			if (mode & S_IWGRP)
				grants |= DIR_WRITE;
			if (mode & S_IRGRP)
				grants |= DIR_READ;
		} else {
			gflags = FILE_INHERITANCE;
			if (mode & S_IXGRP)
				grants |= FILE_EXEC;
			if (mode & S_IWGRP)
				grants |= FILE_WRITE;
			if (mode & S_IRGRP)
				grants |= FILE_READ;
		}

		/* a possible ACE to deny group what it would get from world */
		/* or administrator, unless owner is administrator or group */

		denials = const_cpu_to_le32(0);
		pdace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
		if (!adminowns && !groupowns) {
			if (isdir) {
				pdace->flags = DIR_INHERITANCE;
				if (mode & S_IXOTH)
					denials |= DIR_EXEC;
				if (mode & S_IWOTH)
					denials |= DIR_WRITE;
				if (mode & S_IROTH)
					denials |= DIR_READ;
			} else {
				pdace->flags = FILE_INHERITANCE;
				if (mode & S_IXOTH)
					denials |= FILE_EXEC;
				if (mode & S_IWOTH)
					denials |= FILE_WRITE;
				if (mode & S_IROTH)
					denials |= FILE_READ;
			}
			denials &= ~(grants | OWNER_RIGHTS);
			if (denials) {
				pdace->type = ACCESS_DENIED_ACE_TYPE;
				pdace->size = cpu_to_le16(gsidsz + 8);
				pdace->mask = denials;
				memcpy((char*)&pdace->sid, gsid, gsidsz);
				pos += gsidsz + 8;
				acecnt++;
			}
		}

		if (adminowns
		   || groupowns
		   || ((mode >> 3) & ~mode & 7)) {
				/* now insert grants to group */
				/* if more rights than other */
			pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
			pgace->type = ACCESS_ALLOWED_ACE_TYPE;
			pgace->flags = gflags;
			pgace->size = cpu_to_le16(gsidsz + 8);
			pgace->mask = grants;
			memcpy((char*)&pgace->sid, gsid, gsidsz);
			pos += gsidsz + 8;
			acecnt++;
		}
	}

	/* an ACE for world users */

	pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
	pgace->type = ACCESS_ALLOWED_ACE_TYPE;
	grants = WORLD_RIGHTS;
	if (isdir) {
		pgace->flags = DIR_INHERITANCE;
		if (mode & S_IXOTH)
			grants |= DIR_EXEC;
		if (mode & S_IWOTH)
			grants |= DIR_WRITE;
		if (mode & S_IROTH)
			grants |= DIR_READ;
	} else {
		pgace->flags = FILE_INHERITANCE;
		if (mode & S_IXOTH)
			grants |= FILE_EXEC;
		if (mode & S_IWOTH)
			grants |= FILE_WRITE;
		if (mode & S_IROTH)
			grants |= FILE_READ;
	}
	pgace->size = cpu_to_le16(wsidsz + 8);
	pgace->mask = grants;
	memcpy((char*)&pgace->sid, worldsid, wsidsz);
	pos += wsidsz + 8;
	acecnt++;

	/* an ACE for administrators */
	/* always full access */

	pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
	pgace->type = ACCESS_ALLOWED_ACE_TYPE;
	if (isdir)
		pgace->flags = DIR_INHERITANCE;
	else
		pgace->flags = FILE_INHERITANCE;
	pgace->size = cpu_to_le16(asidsz + 8);
	grants = OWNER_RIGHTS | FILE_READ | FILE_WRITE | FILE_EXEC;
	pgace->mask = grants;
	memcpy((char*)&pgace->sid, adminsid, asidsz);
	pos += asidsz + 8;
	acecnt++;

	/* an ACE for system (needed ?) */
	/* always full access */

	pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
	pgace->type = ACCESS_ALLOWED_ACE_TYPE;
	if (isdir)
		pgace->flags = DIR_INHERITANCE;
	else
		pgace->flags = FILE_INHERITANCE;
	pgace->size = cpu_to_le16(ssidsz + 8);
	grants = OWNER_RIGHTS | FILE_READ | FILE_WRITE | FILE_EXEC;
	pgace->mask = grants;
	memcpy((char*)&pgace->sid, systemsid, ssidsz);
	pos += ssidsz + 8;
	acecnt++;

	/* a null ACE to hold special flags */
	/* using the same representation as cygwin */

	if (mode & (S_ISVTX | S_ISGID | S_ISUID)) {
		nsidsz = ntfs_sid_size(nullsid);
		pgace = (ACCESS_ALLOWED_ACE*)&secattr[offs + pos];
		pgace->type = ACCESS_ALLOWED_ACE_TYPE;
		pgace->flags = NO_PROPAGATE_INHERIT_ACE;
		pgace->size = cpu_to_le16(nsidsz + 8);
		grants = const_cpu_to_le32(0);
		if (mode & S_ISUID)
			grants |= FILE_APPEND_DATA;
		if (mode & S_ISGID)
			grants |= FILE_WRITE_DATA;
		if (mode & S_ISVTX)
			grants |= FILE_READ_DATA;
		pgace->mask = grants;
		memcpy((char*)&pgace->sid, nullsid, nsidsz);
		pos += nsidsz + 8;
		acecnt++;
	}

	/* fix ACL header */
	pacl->size = cpu_to_le16(pos);
	pacl->ace_count = cpu_to_le16(acecnt);
	return (pos);
}

#if POSIXACLS

/*
 *		Build a full security descriptor from a Posix ACL
 *	returns descriptor in allocated memory, must free() after use
 */

char *ntfs_build_descr_posix(struct MAPPING* const mapping[],
			struct POSIX_SECURITY *pxdesc,
			int isdir, const SID *usid, const SID *gsid)
{
	int newattrsz;
	SECURITY_DESCRIPTOR_RELATIVE *pnhead;
	char *newattr;
	int aclsz;
	int usidsz;
	int gsidsz;
	int wsidsz;
	int asidsz;
	int ssidsz;
	int k;

	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);
	wsidsz = ntfs_sid_size(worldsid);
	asidsz = ntfs_sid_size(adminsid);
	ssidsz = ntfs_sid_size(systemsid);

	/* allocate enough space for the new security attribute */
	newattrsz = sizeof(SECURITY_DESCRIPTOR_RELATIVE)	/* header */
	    + usidsz + gsidsz	/* usid and gsid */
	    + sizeof(ACL)	/* acl header */
	    + 2*(8 + usidsz)	/* two possible ACE for user */
	    + 3*(8 + gsidsz)	/* three possible ACE for group and mask */
	    + 8 + wsidsz	/* one ACE for world */
	    + 8 + asidsz	/* one ACE for admin */
	    + 8 + ssidsz;	/* one ACE for system */
	if (isdir)			/* a world denial for directories */
		newattrsz += 8 + wsidsz;
	if (pxdesc->mode & 07000)	/* a NULL ACE for special modes */
		newattrsz += 8 + ntfs_sid_size(nullsid);
				/* account for non-owning users and groups */
	for (k=0; k<pxdesc->acccnt; k++) {
		if ((pxdesc->acl.ace[k].tag == POSIX_ACL_USER)
		    || (pxdesc->acl.ace[k].tag == POSIX_ACL_GROUP))
			newattrsz += 3*40; /* fixme : maximum size */
	}
				/* account for default ACE's */
	newattrsz += 2*40*pxdesc->defcnt;  /* fixme : maximum size */
	newattr = (char*)ntfs_malloc(newattrsz);
	if (newattr) {
		/* build the main header part */
		pnhead = (SECURITY_DESCRIPTOR_RELATIVE*)newattr;
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
		/* build permissions */
		aclsz = buildacls_posix(mapping,newattr,
			  sizeof(SECURITY_DESCRIPTOR_RELATIVE),
			  pxdesc, isdir, usid, gsid);
		if (aclsz && ((int)(sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				+ aclsz + usidsz + gsidsz) <= newattrsz)) {
			/* append usid and gsid */
			memcpy(&newattr[sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz], usid, usidsz);
			memcpy(&newattr[sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				+ aclsz + usidsz], gsid, gsidsz);
			/* positions of ACL, USID and GSID into header */
			pnhead->owner =
			    cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz);
			pnhead->group =
			    cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz + usidsz);
			pnhead->sacl = const_cpu_to_le32(0);
			pnhead->dacl =
			    const_cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE));
		} else {
			/* ACL failure (errno set) or overflow */
			free(newattr);
			newattr = (char*)NULL;
			if (aclsz) {
				/* hope error was detected before overflowing */
				ntfs_log_error("Security descriptor is longer than expected\n");
				errno = EIO;
			}
		}
	} else
		errno = ENOMEM;
	return (newattr);
}

#endif /* POSIXACLS */

/*
 *		Build a full security descriptor
 *	returns descriptor in allocated memory, must free() after use
 */

char *ntfs_build_descr(mode_t mode,
			int isdir, const SID * usid, const SID * gsid)
{
	int newattrsz;
	SECURITY_DESCRIPTOR_RELATIVE *pnhead;
	char *newattr;
	int aclsz;
	int usidsz;
	int gsidsz;
	int wsidsz;
	int asidsz;
	int ssidsz;

	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);
	wsidsz = ntfs_sid_size(worldsid);
	asidsz = ntfs_sid_size(adminsid);
	ssidsz = ntfs_sid_size(systemsid);

	/* allocate enough space for the new security attribute */
	newattrsz = sizeof(SECURITY_DESCRIPTOR_RELATIVE)	/* header */
	    + usidsz + gsidsz	/* usid and gsid */
	    + sizeof(ACL)	/* acl header */
	    + 2*(8 + usidsz)	/* two possible ACE for user */
	    + 2*(8 + gsidsz)	/* two possible ACE for group */
	    + 8 + wsidsz	/* one ACE for world */
	    + 8 + asidsz 	/* one ACE for admin */
	    + 8 + ssidsz;	/* one ACE for system */
	if (isdir)			/* a world denial for directories */
		newattrsz += 8 + wsidsz;
	if (mode & 07000)	/* a NULL ACE for special modes */
		newattrsz += 8 + ntfs_sid_size(nullsid);
	newattr = (char*)ntfs_malloc(newattrsz);
	if (newattr) {
		/* build the main header part */
		pnhead = (SECURITY_DESCRIPTOR_RELATIVE*) newattr;
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
		/* build permissions */
		aclsz = buildacls(newattr,
			  sizeof(SECURITY_DESCRIPTOR_RELATIVE),
			  mode, isdir, usid, gsid);
		if (((int)sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				+ aclsz + usidsz + gsidsz) <= newattrsz) {
			/* append usid and gsid */
			memcpy(&newattr[sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz], usid, usidsz);
			memcpy(&newattr[sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				+ aclsz + usidsz], gsid, gsidsz);
			/* positions of ACL, USID and GSID into header */
			pnhead->owner =
			    cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz);
			pnhead->group =
			    cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE)
				 + aclsz + usidsz);
			pnhead->sacl = const_cpu_to_le32(0);
			pnhead->dacl =
			    const_cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE));
		} else {
			/* hope error was detected before overflowing */
			free(newattr);
			newattr = (char*)NULL;
			ntfs_log_error("Security descriptor is longer than expected\n");
			errno = EIO;
		}
	} else
		errno = ENOMEM;
	return (newattr);
}

/*
 *		Create a mode_t permission set
 *	from owner, group and world grants as represented in ACEs
 */

static int merge_permissions(BOOL isdir,
		le32 owner, le32 group, le32 world, le32 special)

{
	int perm;

	perm = 0;
	/* build owner permission */
	if (owner) {
		if (isdir) {
			/* exec if any of list, traverse */
			if (owner & DIR_GEXEC)
				perm |= S_IXUSR;
			/* write if any of addfile, adddir, delchild */
			if (owner & DIR_GWRITE)
				perm |= S_IWUSR;
			/* read if any of list */
			if (owner & DIR_GREAD)
				perm |= S_IRUSR;
		} else {
			/* exec if execute or generic execute */
			if (owner & FILE_GEXEC)
				perm |= S_IXUSR;
			/* write if any of writedata or generic write */
			if (owner & FILE_GWRITE)
				perm |= S_IWUSR;
			/* read if any of readdata or generic read */
			if (owner & FILE_GREAD)
				perm |= S_IRUSR;
		}
	}
	/* build group permission */
	if (group) {
		if (isdir) {
			/* exec if any of list, traverse */
			if (group & DIR_GEXEC)
				perm |= S_IXGRP;
			/* write if any of addfile, adddir, delchild */
			if (group & DIR_GWRITE)
				perm |= S_IWGRP;
			/* read if any of list */
			if (group & DIR_GREAD)
				perm |= S_IRGRP;
		} else {
			/* exec if execute */
			if (group & FILE_GEXEC)
				perm |= S_IXGRP;
			/* write if any of writedata, appenddata */
			if (group & FILE_GWRITE)
				perm |= S_IWGRP;
			/* read if any of readdata */
			if (group & FILE_GREAD)
				perm |= S_IRGRP;
		}
	}
	/* build world permission */
	if (world) {
		if (isdir) {
			/* exec if any of list, traverse */
			if (world & DIR_GEXEC)
				perm |= S_IXOTH;
			/* write if any of addfile, adddir, delchild */
			if (world & DIR_GWRITE)
				perm |= S_IWOTH;
			/* read if any of list */
			if (world & DIR_GREAD)
				perm |= S_IROTH;
		} else {
			/* exec if execute */
			if (world & FILE_GEXEC)
				perm |= S_IXOTH;
			/* write if any of writedata, appenddata */
			if (world & FILE_GWRITE)
				perm |= S_IWOTH;
			/* read if any of readdata */
			if (world & FILE_GREAD)
				perm |= S_IROTH;
		}
	}
	/* build special permission flags */
	if (special) {
		if (special & FILE_APPEND_DATA)
			perm |= S_ISUID;
		if (special & FILE_WRITE_DATA)
			perm |= S_ISGID;
		if (special & FILE_READ_DATA)
			perm |= S_ISVTX;
	}
	return (perm);
}

#if POSIXACLS

/*
 *		Normalize a Posix ACL either from a sorted raw set of
 *		access ACEs or default ACEs
 *		(standard case : different owner, group and administrator)
 */

static int norm_std_permissions_posix(struct POSIX_SECURITY *posix_desc,
		BOOL groupowns, int start, int count, int target)
{
	int j,k;
	s32 id;
	u16 tag;
	u16 tagsset;
	struct POSIX_ACE *pxace;
	mode_t grantgrps;
	mode_t grantwrld;
	mode_t denywrld;
	mode_t allow;
	mode_t deny;
	mode_t perms;
	mode_t mode;

	mode = 0;
	tagsset = 0;
		/*
		 * Determine what is granted to some group or world
		 * Also get denials to world which are meant to prevent
		 * execution flags to be inherited by plain files
		 */
	pxace = posix_desc->acl.ace;
	grantgrps = 0;
	grantwrld = 0;
	denywrld = 0;
	for (j=start; j<(start + count); j++) {
		if (pxace[j].perms & POSIX_PERM_DENIAL) {
				/* deny world exec unless for default */
			if ((pxace[j].tag == POSIX_ACL_OTHER)
			&& !start)
				denywrld = pxace[j].perms;
		} else {
			switch (pxace[j].tag) {
			case POSIX_ACL_GROUP_OBJ :
				grantgrps |= pxace[j].perms;
				break;
			case POSIX_ACL_GROUP :
				if (pxace[j].id)
					grantgrps |= pxace[j].perms;
				break;
			case POSIX_ACL_OTHER :
				grantwrld = pxace[j].perms;
				break;
			default :
				break;
			}
		}
	}
		/*
		 * Collect groups of ACEs related to the same id
		 * and determine what is granted and what is denied.
		 * It is important the ACEs have been sorted
		 */
	j = start;
	k = target;
	while (j < (start + count)) {
		tag = pxace[j].tag;
		id = pxace[j].id;
		if (pxace[j].perms & POSIX_PERM_DENIAL) {
			deny = pxace[j].perms | denywrld;
			allow = 0;
		} else {
			deny = denywrld;
			allow = pxace[j].perms;
		}
		j++;
		while ((j < (start + count))
		    && (pxace[j].tag == tag)
		    && (pxace[j].id == id)) {
			if (pxace[j].perms & POSIX_PERM_DENIAL)
				deny |= pxace[j].perms;
			else
				allow |= pxace[j].perms;
			j++;
		}
			/*
			 * Build the permissions equivalent to grants and denials
			 */
		if (groupowns) {
			if (tag == POSIX_ACL_MASK)
				perms = ~deny;
			else
				perms = allow & ~deny;
		} else
			switch (tag) {
			case POSIX_ACL_USER_OBJ :
				perms = (allow | grantgrps | grantwrld) & ~deny;
				break;
			case POSIX_ACL_USER :
				if (id)
					perms = (allow | grantgrps | grantwrld)
						& ~deny;
				else
					perms = allow;
				break;
			case POSIX_ACL_GROUP_OBJ :
				perms = (allow | grantwrld) & ~deny;
				break;
			case POSIX_ACL_GROUP :
				if (id)
					perms = (allow | grantwrld) & ~deny;
				else
					perms = allow;
				break;
			case POSIX_ACL_MASK :
				perms = ~deny;
				break;
			default :
				perms = allow & ~deny;
				break;
			}
			/*
			 * Store into a Posix ACE
			 */
		if (tag != POSIX_ACL_SPECIAL) {
			pxace[k].tag = tag;
			pxace[k].id = id;
			pxace[k].perms = perms
				 & (POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X);
			tagsset |= tag;
			k++;
		}
		switch (tag) {
		case POSIX_ACL_USER_OBJ :
			mode |= ((perms & 7) << 6);
			break;
		case POSIX_ACL_GROUP_OBJ :
		case POSIX_ACL_MASK :
			mode = (mode & 07707) | ((perms & 7) << 3);
			break;
		case POSIX_ACL_OTHER :
			mode |= perms & 7;
			break;
		case POSIX_ACL_SPECIAL :
			mode |= (perms & (S_ISVTX | S_ISUID | S_ISGID));
			break;
		default :
			break;
		}
	}
	if (!start) { /* not satisfactory */
		posix_desc->mode = mode;
		posix_desc->tagsset = tagsset;
	}
	return (k - target);
}

#endif /* POSIXACLS */

/*
 *		Interpret an ACL and extract meaningful grants
 *		(standard case : different owner, group and administrator)
 */

static int build_std_permissions(const char *securattr,
		const SID *usid, const SID *gsid, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	int offdacl;
	int offace;
	int acecnt;
	int nace;
	BOOL noown;
	le32 special;
	le32 allowown, allowgrp, allowall;
	le32 denyown, denygrp, denyall;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	pacl = (const ACL*)&securattr[offdacl];
	special = const_cpu_to_le32(0);
	allowown = allowgrp = allowall = const_cpu_to_le32(0);
	denyown = denygrp = denyall = const_cpu_to_le32(0);
	noown = TRUE;
	if (offdacl) {
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
	} else {
		acecnt = 0;
		offace = 0;
	}
	for (nace = 0; nace < acecnt; nace++) {
		pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
		if (!(pace->flags & INHERIT_ONLY_ACE)) {
			if (ntfs_same_sid(usid, &pace->sid)
			  || ntfs_same_sid(ownersid, &pace->sid)) {
				noown = FALSE;
				if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
					allowown |= pace->mask;
				else if (pace->type == ACCESS_DENIED_ACE_TYPE)
					denyown |= pace->mask;
				} else
				if (ntfs_same_sid(gsid, &pace->sid)
				    && !(pace->mask & WRITE_OWNER)) {
					if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
						allowgrp |= pace->mask;
					else if (pace->type == ACCESS_DENIED_ACE_TYPE)
						denygrp |= pace->mask;
				} else
					if (is_world_sid((const SID*)&pace->sid)) {
						if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
							allowall |= pace->mask;
						else
							if (pace->type == ACCESS_DENIED_ACE_TYPE)
								denyall |= pace->mask;
					} else
					if ((ntfs_same_sid((const SID*)&pace->sid,nullsid))
					   && (pace->type == ACCESS_ALLOWED_ACE_TYPE))
						special |= pace->mask;
			}
			offace += le16_to_cpu(pace->size);
		}
		/*
		 * No indication about owner's rights : grant basic rights
		 * This happens for files created by Windows in directories
		 * created by Linux and owned by root, because Windows
		 * merges the admin ACEs
		 */
	if (noown)
		allowown = (FILE_READ_DATA | FILE_WRITE_DATA | FILE_EXECUTE);
		/*
		 *  Add to owner rights granted to group or world
		 * unless denied personaly, and add to group rights
		 * granted to world unless denied specifically
		 */
	allowown |= (allowgrp | allowall);
	allowgrp |= allowall;
	return (merge_permissions(isdir,
				allowown & ~(denyown | denyall),
				allowgrp & ~(denygrp | denyall),
				allowall & ~denyall,
				special));
}

/*
 *		Interpret an ACL and extract meaningful grants
 *		(special case : owner and group are the same,
 *		and not administrator)
 */

static int build_owngrp_permissions(const char *securattr,
			const SID *usid, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	int offdacl;
	int offace;
	int acecnt;
	int nace;
	le32 special;
	BOOL grppresent;
	le32 allowown, allowgrp, allowall;
	le32 denyown, denygrp, denyall;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	pacl = (const ACL*)&securattr[offdacl];
	special = const_cpu_to_le32(0);
	allowown = allowgrp = allowall = const_cpu_to_le32(0);
	denyown = denygrp = denyall = const_cpu_to_le32(0);
	grppresent = FALSE;
	if (offdacl) {
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
	} else
		acecnt = 0;
	for (nace = 0; nace < acecnt; nace++) {
		pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
		if (!(pace->flags & INHERIT_ONLY_ACE)) {
			if ((ntfs_same_sid(usid, &pace->sid)
			   || ntfs_same_sid(ownersid, &pace->sid))
			    && (pace->mask & WRITE_OWNER)) {
				if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
					allowown |= pace->mask;
				} else
				if (ntfs_same_sid(usid, &pace->sid)
				   && (!(pace->mask & WRITE_OWNER))) {
					if (pace->type == ACCESS_ALLOWED_ACE_TYPE) {
						allowgrp |= pace->mask;
						grppresent = TRUE;
					}
				} else
					if (is_world_sid((const SID*)&pace->sid)) {
						if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
							allowall |= pace->mask;
						else
							if (pace->type == ACCESS_DENIED_ACE_TYPE)
								denyall |= pace->mask;
					} else
					if ((ntfs_same_sid((const SID*)&pace->sid,nullsid))
					   && (pace->type == ACCESS_ALLOWED_ACE_TYPE))
						special |= pace->mask;
			}
			offace += le16_to_cpu(pace->size);
		}
	if (!grppresent)
		allowgrp = allowall;
	return (merge_permissions(isdir,
				allowown & ~(denyown | denyall),
				allowgrp & ~(denygrp | denyall),
				allowall & ~denyall,
				special));
}

#if POSIXACLS

/*
 *		Normalize a Posix ACL either from a sorted raw set of
 *		access ACEs or default ACEs
 *		(special case : owner or/and group is administrator)
 */

static int norm_ownadmin_permissions_posix(struct POSIX_SECURITY *posix_desc,
		int start, int count, int target)
{
	int j,k;
	s32 id;
	u16 tag;
	u16 tagsset;
	struct POSIX_ACE *pxace;
	int acccnt;
	mode_t denywrld;
	mode_t allow;
	mode_t deny;
	mode_t perms;
	mode_t mode;

	mode = 0;
	pxace = posix_desc->acl.ace;
	acccnt = posix_desc->acccnt;
	tagsset = 0;
	denywrld = 0;
		/*
		 * Get denials to world which are meant to prevent
		 * execution flags to be inherited by plain files
		 */
	for (j=start; j<(start + count); j++) {
		if (pxace[j].perms & POSIX_PERM_DENIAL) {
				/* deny world exec not for default */
			if ((pxace[j].tag == POSIX_ACL_OTHER)
			&& !start)
				denywrld = pxace[j].perms;
		}
	}
		/*
		 * Collect groups of ACEs related to the same id
		 * and determine what is granted (denials are ignored)
		 * It is important the ACEs have been sorted
		 */
	j = start;
	k = target;
	deny = 0;
	while (j < (start + count)) {
		allow = 0;
		tag = pxace[j].tag;
		id = pxace[j].id;
		if (tag == POSIX_ACL_MASK) {
			deny = pxace[j].perms;
			j++;
			while ((j < (start + count))
			    && (pxace[j].tag == POSIX_ACL_MASK))
				j++;
		} else {
			if (!(pxace[j].perms & POSIX_PERM_DENIAL))
				allow = pxace[j].perms;
			j++;
			while ((j < (start + count))
			    && (pxace[j].tag == tag)
			    && (pxace[j].id == id)) {
				if (!(pxace[j].perms & POSIX_PERM_DENIAL))
					allow |= pxace[j].perms;
				j++;
			}
		}

			/*
			 * Store the grants into a Posix ACE
			 */
		if (tag == POSIX_ACL_MASK)
			perms = ~deny;
		else
			perms = allow & ~denywrld;
		if (tag != POSIX_ACL_SPECIAL) {
			pxace[k].tag = tag;
			pxace[k].id = id;
			pxace[k].perms = perms
				 & (POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X);
			tagsset |= tag;
			k++;
		}
		switch (tag) {
		case POSIX_ACL_USER_OBJ :
			mode |= ((perms & 7) << 6);
			break;
		case POSIX_ACL_GROUP_OBJ :
		case POSIX_ACL_MASK :
			mode = (mode & 07707) | ((perms & 7) << 3);
			break;
		case POSIX_ACL_OTHER :
			mode |= perms & 7;
			break;
		case POSIX_ACL_SPECIAL :
			mode |= perms & (S_ISVTX | S_ISUID | S_ISGID);
			break;
		default :
			break;
		}
	}
	if (!start) { /* not satisfactory */
		posix_desc->mode = mode;
		posix_desc->tagsset = tagsset;
	}
	return (k - target);
}

#endif /* POSIXACLS */

/*
 *		Interpret an ACL and extract meaningful grants
 *		(special case : owner or/and group is administrator)
 */


static int build_ownadmin_permissions(const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	int offdacl;
	int offace;
	int acecnt;
	int nace;
	BOOL firstapply;
	int isforeign;
	le32 special;
	le32 allowown, allowgrp, allowall;
	le32 denyown, denygrp, denyall;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	pacl = (const ACL*)&securattr[offdacl];
	special = const_cpu_to_le32(0);
	allowown = allowgrp = allowall = const_cpu_to_le32(0);
	denyown = denygrp = denyall = const_cpu_to_le32(0);
	if (offdacl) {
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
	} else {
		acecnt = 0;
		offace = 0;
	}
	firstapply = TRUE;
	isforeign = 3;
	for (nace = 0; nace < acecnt; nace++) {
		pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
		if (!(pace->flags & INHERIT_ONLY_ACE)
		   && !(~pace->mask & (ROOT_OWNER_UNMARK | ROOT_GROUP_UNMARK))) {
			if ((ntfs_same_sid(usid, &pace->sid)
			   || ntfs_same_sid(ownersid, &pace->sid))
			     && (((pace->mask & WRITE_OWNER) && firstapply))) {
				if (pace->type == ACCESS_ALLOWED_ACE_TYPE) {
					allowown |= pace->mask;
					isforeign &= ~1;
				} else
					if (pace->type == ACCESS_DENIED_ACE_TYPE)
						denyown |= pace->mask;
				} else
				    if (ntfs_same_sid(gsid, &pace->sid)
					&& (!(pace->mask & WRITE_OWNER))) {
						if (pace->type == ACCESS_ALLOWED_ACE_TYPE) {
							allowgrp |= pace->mask;
							isforeign &= ~2;
						} else
							if (pace->type == ACCESS_DENIED_ACE_TYPE)
								denygrp |= pace->mask;
					} else if (is_world_sid((const SID*)&pace->sid)) {
						if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
							allowall |= pace->mask;
						else
							if (pace->type == ACCESS_DENIED_ACE_TYPE)
								denyall |= pace->mask;
					}
			firstapply = FALSE;
			} else
				if (!(pace->flags & INHERIT_ONLY_ACE))
					if ((ntfs_same_sid((const SID*)&pace->sid,nullsid))
					   && (pace->type == ACCESS_ALLOWED_ACE_TYPE))
						special |= pace->mask;
			offace += le16_to_cpu(pace->size);
		}
	if (isforeign) {
		allowown |= (allowgrp | allowall);
		allowgrp |= allowall;
	}
	return (merge_permissions(isdir,
				allowown & ~(denyown | denyall),
				allowgrp & ~(denygrp | denyall),
				allowall & ~denyall,
				special));
}

#if OWNERFROMACL

/*
 *		Define the owner of a file as the first user allowed
 *	to change the owner, instead of the user defined as owner.
 *
 *	This produces better approximations for files written by a
 *	Windows user in an inheritable directory owned by another user,
 *	as the access rights are inheritable but the ownership is not.
 *
 *	An important case is the directories "Documents and Settings/user"
 *	which the users must have access to, though Windows considers them
 *	as owned by administrator.
 */

const SID *ntfs_acl_owner(const char *securattr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const SID *usid;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	int offdacl;
	int offace;
	int acecnt;
	int nace;
	BOOL found;

	found = FALSE;
	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	if (offdacl) {
		pacl = (const ACL*)&securattr[offdacl];
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
		nace = 0;
		do {
			pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
			if ((pace->mask & WRITE_OWNER)
			   && (pace->type == ACCESS_ALLOWED_ACE_TYPE)
			   && ntfs_is_user_sid(&pace->sid))
				found = TRUE;
			offace += le16_to_cpu(pace->size);
		} while (!found && (++nace < acecnt));
	}
	if (found)
		usid = &pace->sid;
	else
		usid = (const SID*)&securattr[le32_to_cpu(phead->owner)];
	return (usid);
}

#else

/*
 *		Special case for files owned by administrator with full
 *	access granted to a mapped user : consider this user as the tenant
 *	of the file.
 *
 *	This situation cannot be represented with Linux concepts and can
 *	only be found for files or directories created by Windows.
 *	Typical situation : directory "Documents and Settings/user" which
 *	is on the path to user's files and must be given access to user
 *	only.
 *
 *	Check file is owned by administrator and no user has rights before
 *	calling.
 *	Returns the uid of tenant or zero if none
 */


static uid_t find_tenant(struct MAPPING *const mapping[],
			const char *securattr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	int offdacl;
	int offace;
	int acecnt;
	int nace;
	uid_t tid;
	uid_t xid;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	pacl = (const ACL*)&securattr[offdacl];
	tid = 0;
	if (offdacl) {
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
	} else
		acecnt = 0;
	for (nace = 0; nace < acecnt; nace++) {
		pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
		if ((pace->type == ACCESS_ALLOWED_ACE_TYPE)
		   && (pace->mask & DIR_WRITE)) {
			xid = NTFS_FIND_USER(mapping[MAPUSERS], &pace->sid);
			if (xid) tid = xid;
		}
		offace += le16_to_cpu(pace->size);
	}
	return (tid);
}

#endif /* OWNERFROMACL */

#if POSIXACLS

/*
 *		Build Posix permissions from an ACL
 *	returns a pointer to the requested permissions
 *	or a null pointer (with errno set) if there is a problem
 *
 *	If the NTFS ACL was created according to our rules, the retrieved
 *	Posix ACL should be the exact ACL which was set. However if
 *	the NTFS ACL was built by a different tool, the result could
 *	be a a poor approximation of what was expected
 */

struct POSIX_SECURITY *ntfs_build_permissions_posix(
			struct MAPPING *const mapping[],
			const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	struct POSIX_SECURITY *pxdesc;
	const ACL *pacl;
	const ACCESS_ALLOWED_ACE *pace;
	struct POSIX_ACE *pxace;
	struct {
		uid_t prevuid;
		gid_t prevgid;
		int groupmasks;
		s16 tagsset;
		BOOL gotowner;
		BOOL gotownermask;
		BOOL gotgroup;
		mode_t permswrld;
	} ctx[2], *pctx;
	int offdacl;
	int offace;
	int alloccnt;
	int acecnt;
	uid_t uid;
	gid_t gid;
	int i,j;
	int k,l;
	BOOL ignore;
	BOOL adminowns;
	BOOL groupowns;
	BOOL firstinh;
	BOOL genericinh;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	offdacl = le32_to_cpu(phead->dacl);
	if (offdacl) {
		pacl = (const ACL*)&securattr[offdacl];
		acecnt = le16_to_cpu(pacl->ace_count);
		offace = offdacl + sizeof(ACL);
	} else {
		acecnt = 0;
		offace = 0;
	}
	adminowns = FALSE;
	groupowns = ntfs_same_sid(gsid,usid);
	firstinh = FALSE;
	genericinh = FALSE;
		/*
		 * Build a raw posix security descriptor
		 * by just translating permissions and ids
		 * Add 2 to the count of ACE to be able to insert
		 * a group ACE later in access and default ACLs
		 * and add 2 more to be able to insert ACEs for owner
		 * and 2 more for other
		 */
	alloccnt = acecnt + 6;
	pxdesc = (struct POSIX_SECURITY*)malloc(
				sizeof(struct POSIX_SECURITY)
				+ alloccnt*sizeof(struct POSIX_ACE));
	k = 0;
	l = alloccnt;
	for (i=0; i<2; i++) {
		pctx = &ctx[i];
		pctx->permswrld = 0;
		pctx->prevuid = -1;
		pctx->prevgid = -1;
		pctx->groupmasks = 0;
		pctx->tagsset = 0;
		pctx->gotowner = FALSE;
		pctx->gotgroup = FALSE;
		pctx->gotownermask = FALSE;
	}
	for (j=0; j<acecnt; j++) {
		pace = (const ACCESS_ALLOWED_ACE*)&securattr[offace];
		if (pace->flags & INHERIT_ONLY_ACE) {
			pxace = &pxdesc->acl.ace[l - 1];
			pctx = &ctx[1];
		} else {
			pxace = &pxdesc->acl.ace[k];
			pctx = &ctx[0];
		}
		ignore = FALSE;
			/*
			 * grants for root as a designated user or group
			 */
		if ((~pace->mask & (ROOT_OWNER_UNMARK | ROOT_GROUP_UNMARK))
		   && (pace->type == ACCESS_ALLOWED_ACE_TYPE)
		   && ntfs_same_sid(&pace->sid, adminsid)) {
			pxace->tag = (pace->mask & ROOT_OWNER_UNMARK ? POSIX_ACL_GROUP : POSIX_ACL_USER);
			pxace->id = 0;
			if ((pace->mask & (GENERIC_ALL | WRITE_OWNER))
			   && (pace->flags & INHERIT_ONLY_ACE))
				ignore = genericinh = TRUE;
		} else
		if (ntfs_same_sid(usid, &pace->sid)) {
			pxace->id = -1;
				/*
				 * Owner has no write-owner right :
				 * a group was defined same as owner
				 * or admin was owner or group :
				 * denials are meant to owner
				 * and grants are meant to group
				 */
			if (!(pace->mask & (WRITE_OWNER | GENERIC_ALL))
			    && (pace->type == ACCESS_ALLOWED_ACE_TYPE)) {
				if (ntfs_same_sid(gsid,usid)) {
					pxace->tag = POSIX_ACL_GROUP_OBJ;
					pxace->id = -1;
				} else {
					if (ntfs_same_sid(&pace->sid,usid))
						groupowns = TRUE;
					gid = NTFS_FIND_GROUP(mapping[MAPGROUPS],&pace->sid);
					if (gid) {
						pxace->tag = POSIX_ACL_GROUP;
						pxace->id = gid;
						pctx->prevgid = gid;
					} else {
					uid = NTFS_FIND_USER(mapping[MAPUSERS],&pace->sid);
					if (uid) {
						pxace->tag = POSIX_ACL_USER;
						pxace->id = uid;
					} else
						ignore = TRUE;
					}
				}
			} else {
				/*
				 * when group owns, late denials for owner
				 * mean group mask
				 */
				if ((pace->type == ACCESS_DENIED_ACE_TYPE)
				    && (pace->mask & WRITE_OWNER)) {
					pxace->tag = POSIX_ACL_MASK;
					pctx->gotownermask = TRUE;
					if (pctx->gotowner)
						pctx->groupmasks++;
				} else {
					if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
						pctx->gotowner = TRUE;
					if (pctx->gotownermask && !pctx->gotowner) {
						uid = NTFS_FIND_USER(mapping[MAPUSERS],&pace->sid);
						pxace->id = uid;
						pxace->tag = POSIX_ACL_USER;
					} else
						pxace->tag = POSIX_ACL_USER_OBJ;
						/* system ignored, and admin */
						/* ignored at first position */
					if (pace->flags & INHERIT_ONLY_ACE) {
						if ((firstinh && ntfs_same_sid(&pace->sid,adminsid))
						   || ntfs_same_sid(&pace->sid,systemsid))
							ignore = TRUE;
						if (!firstinh) {
							firstinh = TRUE;
						}
					} else {
						if ((adminowns && ntfs_same_sid(&pace->sid,adminsid))
						   || ntfs_same_sid(&pace->sid,systemsid))
							ignore = TRUE;
						if (ntfs_same_sid(usid,adminsid))
							adminowns = TRUE;
					}
				}
			}
		} else if (ntfs_same_sid(gsid, &pace->sid)) {
			if ((pace->type == ACCESS_DENIED_ACE_TYPE)
			    && (pace->mask & WRITE_OWNER)) {
				pxace->tag = POSIX_ACL_MASK;
				pxace->id = -1;
				if (pctx->gotowner)
					pctx->groupmasks++;
			} else {
				if (pctx->gotgroup || (pctx->groupmasks > 1)) {
					gid = NTFS_FIND_GROUP(mapping[MAPGROUPS],&pace->sid);
					if (gid) {
						pxace->id = gid;
						pxace->tag = POSIX_ACL_GROUP;
						pctx->prevgid = gid;
					} else
						ignore = TRUE;
				} else {
					pxace->id = -1;
					pxace->tag = POSIX_ACL_GROUP_OBJ;
					if (pace->type == ACCESS_ALLOWED_ACE_TYPE)
						pctx->gotgroup = TRUE;
				}

				if (ntfs_same_sid(gsid,adminsid)
				    || ntfs_same_sid(gsid,systemsid)) {
					if (pace->mask & (WRITE_OWNER | GENERIC_ALL))
						ignore = TRUE;
					if (ntfs_same_sid(gsid,adminsid))
						adminowns = TRUE;
					else
						genericinh = ignore;
				}
			}
		} else if (is_world_sid((const SID*)&pace->sid)) {
			pxace->id = -1;
			pxace->tag = POSIX_ACL_OTHER;
			if ((pace->type == ACCESS_DENIED_ACE_TYPE)
			   && (pace->flags & INHERIT_ONLY_ACE))
				ignore = TRUE;
		} else if (ntfs_same_sid((const SID*)&pace->sid,nullsid)) {
			pxace->id = -1;
			pxace->tag = POSIX_ACL_SPECIAL;
		} else {
			uid = NTFS_FIND_USER(mapping[MAPUSERS],&pace->sid);
			if (uid) {
				if ((pace->type == ACCESS_DENIED_ACE_TYPE)
				    && (pace->mask & WRITE_OWNER)
				    && (pctx->prevuid != uid)) {
					pxace->id = -1;
					pxace->tag = POSIX_ACL_MASK;
				} else {
					pxace->id = uid;
					pxace->tag = POSIX_ACL_USER;
				}
				pctx->prevuid = uid;
			} else {
				gid = NTFS_FIND_GROUP(mapping[MAPGROUPS],&pace->sid);
				if (gid) {
					if ((pace->type == ACCESS_DENIED_ACE_TYPE)
					    && (pace->mask & WRITE_OWNER)
					    && (pctx->prevgid != gid)) {
						pxace->tag = POSIX_ACL_MASK;
						pctx->groupmasks++;
					} else {
						pxace->tag = POSIX_ACL_GROUP;
					}
					pxace->id = gid;
					pctx->prevgid = gid;
				} else {
					/*
					 * do not grant rights to unknown
					 * people and do not define root as a
					 * designated user or group
					 */
					ignore = TRUE;
				}
			}
		}
		if (!ignore) {
			pxace->perms = 0;
				/* specific decoding for vtx/uid/gid */
			if (pxace->tag == POSIX_ACL_SPECIAL) {
				if (pace->mask & FILE_APPEND_DATA)
					pxace->perms |= S_ISUID;
				if (pace->mask & FILE_WRITE_DATA)
					pxace->perms |= S_ISGID;
				if (pace->mask & FILE_READ_DATA)
					pxace->perms |= S_ISVTX;
			} else
				if (isdir) {
					if (pace->mask & DIR_GEXEC)
						pxace->perms |= POSIX_PERM_X;
					if (pace->mask & DIR_GWRITE)
						pxace->perms |= POSIX_PERM_W;
					if (pace->mask & DIR_GREAD)
						pxace->perms |= POSIX_PERM_R;
					if ((pace->mask & GENERIC_ALL)
					   && (pace->flags & INHERIT_ONLY_ACE))
						pxace->perms |= POSIX_PERM_X
								| POSIX_PERM_W
								| POSIX_PERM_R;
				} else {
					if (pace->mask & FILE_GEXEC)
						pxace->perms |= POSIX_PERM_X;
					if (pace->mask & FILE_GWRITE)
						pxace->perms |= POSIX_PERM_W;
					if (pace->mask & FILE_GREAD)
						pxace->perms |= POSIX_PERM_R;
				}

			if (pace->type != ACCESS_ALLOWED_ACE_TYPE)
				pxace->perms |= POSIX_PERM_DENIAL;
			else
				if (pxace->tag == POSIX_ACL_OTHER)
					pctx->permswrld = pxace->perms;
			pctx->tagsset |= pxace->tag;
			if (pace->flags & INHERIT_ONLY_ACE) {
				l--;
			} else {
				k++;
			}
		}
		offace += le16_to_cpu(pace->size);
	}
		/*
		 * Create world perms if none (both lists)
		 */
	for (i=0; i<2; i++)
		if ((genericinh || !i)
		    && !(ctx[i].tagsset & POSIX_ACL_OTHER)) {
			if (i)
				pxace = &pxdesc->acl.ace[--l];
			else
				pxace = &pxdesc->acl.ace[k++];
			pxace->tag = POSIX_ACL_OTHER;
			pxace->id = -1;
			pxace->perms = 0;
			ctx[i].tagsset |= POSIX_ACL_OTHER;
			ctx[i].permswrld = 0;
		}
		/*
		 * Set basic owner perms if none (both lists)
		 * This happens for files created by Windows in directories
		 * created by Linux and owned by root, because Windows
		 * merges the admin ACEs
		 */
	for (i=0; i<2; i++)
		if (!(ctx[i].tagsset & POSIX_ACL_USER_OBJ)
		  && (ctx[i].tagsset & POSIX_ACL_OTHER)) {
			if (i)
				pxace = &pxdesc->acl.ace[--l];
			else
				pxace = &pxdesc->acl.ace[k++];
			pxace->tag = POSIX_ACL_USER_OBJ;
			pxace->id = -1;
			pxace->perms = POSIX_PERM_R | POSIX_PERM_W | POSIX_PERM_X;
			ctx[i].tagsset |= POSIX_ACL_USER_OBJ;
		}
		/*
		 * Duplicate world perms as group_obj perms if none
		 */
	for (i=0; i<2; i++)
		if ((ctx[i].tagsset & POSIX_ACL_OTHER)
		    && !(ctx[i].tagsset & POSIX_ACL_GROUP_OBJ)) {
			if (i)
				pxace = &pxdesc->acl.ace[--l];
			else
				pxace = &pxdesc->acl.ace[k++];
			pxace->tag = POSIX_ACL_GROUP_OBJ;
			pxace->id = -1;
			pxace->perms = ctx[i].permswrld;
			ctx[i].tagsset |= POSIX_ACL_GROUP_OBJ;
		}
		/*
		 * Also duplicate world perms as group perms if they
		 * were converted to mask and not followed by a group entry
		 */
	if (ctx[0].groupmasks) {
		for (j=k-2; j>=0; j--) {
			if ((pxdesc->acl.ace[j].tag == POSIX_ACL_MASK)
			   && (pxdesc->acl.ace[j].id != -1)
			   && ((pxdesc->acl.ace[j+1].tag != POSIX_ACL_GROUP)
			     || (pxdesc->acl.ace[j+1].id
				!= pxdesc->acl.ace[j].id))) {
				pxace = &pxdesc->acl.ace[k];
				pxace->tag = POSIX_ACL_GROUP;
				pxace->id = pxdesc->acl.ace[j].id;
				pxace->perms = ctx[0].permswrld;
				ctx[0].tagsset |= POSIX_ACL_GROUP;
				k++;
			}
			if (pxdesc->acl.ace[j].tag == POSIX_ACL_MASK)
				pxdesc->acl.ace[j].id = -1;
		}
	}
	if (ctx[1].groupmasks) {
		for (j=l; j<(alloccnt-1); j++) {
			if ((pxdesc->acl.ace[j].tag == POSIX_ACL_MASK)
			   && (pxdesc->acl.ace[j].id != -1)
			   && ((pxdesc->acl.ace[j+1].tag != POSIX_ACL_GROUP)
			     || (pxdesc->acl.ace[j+1].id
				!= pxdesc->acl.ace[j].id))) {
				pxace = &pxdesc->acl.ace[l - 1];
				pxace->tag = POSIX_ACL_GROUP;
				pxace->id = pxdesc->acl.ace[j].id;
				pxace->perms = ctx[1].permswrld;
				ctx[1].tagsset |= POSIX_ACL_GROUP;
				l--;
			}
			if (pxdesc->acl.ace[j].tag == POSIX_ACL_MASK)
				pxdesc->acl.ace[j].id = -1;
		}
	}

		/*
		 * Insert default mask if none present and
		 * there are designated users or groups
		 * (the space for it has not beed used)
		 */
	for (i=0; i<2; i++)
		if ((ctx[i].tagsset & (POSIX_ACL_USER | POSIX_ACL_GROUP))
		    && !(ctx[i].tagsset & POSIX_ACL_MASK)) {
			if (i)
				pxace = &pxdesc->acl.ace[--l];
			else
				pxace = &pxdesc->acl.ace[k++];
			pxace->tag = POSIX_ACL_MASK;
			pxace->id = -1;
			pxace->perms = POSIX_PERM_DENIAL;
			ctx[i].tagsset |= POSIX_ACL_MASK;
		}

	if (k > l) {
		ntfs_log_error("Posix descriptor is longer than expected\n");
		errno = EIO;
		free(pxdesc);
		pxdesc = (struct POSIX_SECURITY*)NULL;
	} else {
		pxdesc->acccnt = k;
		pxdesc->defcnt = alloccnt - l;
		pxdesc->firstdef = l;
		pxdesc->tagsset = ctx[0].tagsset;
		pxdesc->acl.version = POSIX_VERSION;
		pxdesc->acl.flags = 0;
		pxdesc->acl.filler = 0;
		ntfs_sort_posix(pxdesc);
		if (adminowns) {
			k = norm_ownadmin_permissions_posix(pxdesc,
					0, pxdesc->acccnt, 0);
			pxdesc->acccnt = k;
			l = norm_ownadmin_permissions_posix(pxdesc,
					pxdesc->firstdef, pxdesc->defcnt, k);
			pxdesc->firstdef = k;
			pxdesc->defcnt = l;
		} else {
			k = norm_std_permissions_posix(pxdesc,groupowns,
					0, pxdesc->acccnt, 0);
			pxdesc->acccnt = k;
			l = norm_std_permissions_posix(pxdesc,groupowns,
					pxdesc->firstdef, pxdesc->defcnt, k);
			pxdesc->firstdef = k;
			pxdesc->defcnt = l;
		}
	}
	if (pxdesc && !ntfs_valid_posix(pxdesc)) {
		ntfs_log_error("Invalid Posix descriptor built\n");
                errno = EIO;
                free(pxdesc);
                pxdesc = (struct POSIX_SECURITY*)NULL;
	}
	return (pxdesc);
}

#endif /* POSIXACLS */

/*
 *		Build unix-style (mode_t) permissions from an ACL
 *	returns the requested permissions
 *	or a negative result (with errno set) if there is a problem
 */

int ntfs_build_permissions(const char *securattr,
			const SID *usid, const SID *gsid, BOOL isdir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	int perm;
	BOOL adminowns;
	BOOL groupowns;

	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
	adminowns = ntfs_same_sid(usid,adminsid)
	         || ntfs_same_sid(gsid,adminsid);
	groupowns = !adminowns && ntfs_same_sid(gsid,usid);
	if (adminowns)
		perm = build_ownadmin_permissions(securattr, usid, gsid, isdir);
	else
		if (groupowns)
			perm = build_owngrp_permissions(securattr, usid, isdir);
		else
			perm = build_std_permissions(securattr, usid, gsid, isdir);
	return (perm);
}

/*
 *		The following must be in some library...
 */

static unsigned long atoul(const char *p)
{				/* must be somewhere ! */
	unsigned long v;

	v = 0;
	while ((*p >= '0') && (*p <= '9'))
		v = v * 10 + (*p++) - '0';
	return (v);
}

/*
 *		Build an internal representation of a SID
 *	Returns a copy in allocated memory if it succeeds
 *	The SID is checked to be a valid user one.
 */

static SID *encodesid(const char *sidstr)
{
	SID *sid;
	int cnt;
	BIGSID bigsid;
	SID *bsid;
	u32 auth;
	const char *p;

	sid = (SID*) NULL;
	if (!strncmp(sidstr, "S-1-", 4)) {
		bsid = (SID*)&bigsid;
		bsid->revision = SID_REVISION;
		p = &sidstr[4];
		auth = atoul(p);
		bsid->identifier_authority.high_part = const_cpu_to_be16(0);
		bsid->identifier_authority.low_part = cpu_to_be32(auth);
		cnt = 0;
		p = strchr(p, '-');
		while (p && (cnt < 8)) {
			p++;
			auth = atoul(p);
			bsid->sub_authority[cnt] = cpu_to_le32(auth);
			p = strchr(p, '-');
			cnt++;
		}
		bsid->sub_authority_count = cnt;
		if ((cnt > 0) && ntfs_valid_sid(bsid) && ntfs_is_user_sid(bsid)) {
			sid = (SID*) ntfs_malloc(4 * cnt + 8);
			if (sid)
				memcpy(sid, bsid, 4 * cnt + 8);
		}
	}
	return (sid);
}

/*
 *			Early logging before the logs are redirected
 *
 *	(not quite satisfactory : this appears before the ntfs-g banner,
 *	and with a different pid)
 */

static void log_early_error(const char *format, ...)
		__attribute__((format(printf, 1, 2)));

static void log_early_error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
#ifdef HAVE_SYSLOG_H
	openlog("ntfs-3g", LOG_PID, LOG_USER);
	ntfs_log_handler_syslog(NULL, NULL, 0,
		NTFS_LOG_LEVEL_ERROR, NULL,
		format, args);
#else
	vfprintf(stderr,format,args);
#endif
	va_end(args);
}


/*
 *		Get a single mapping item from buffer
 *
 *	Always reads a full line, truncating long lines
 *	Refills buffer when exhausted
 *	Returns pointer to item, or NULL when there is no more
 */

static struct MAPLIST *getmappingitem(FILEREADER reader, void *fileid,
		off_t *poffs, char *buf, int *psrc, s64 *psize)
{
	int src;
	int dst;
	char *p;
	char *q;
	char *pu;
	char *pg;
	int gotend;
	struct MAPLIST *item;

	src = *psrc;
	dst = 0;
			/* allocate and get a full line */
	item = (struct MAPLIST*)ntfs_malloc(sizeof(struct MAPLIST));
	if (item) {
		do {
			gotend = 0;
			while ((src < *psize)
			       && (buf[src] != '\n')) {
				if (dst < LINESZ)
					item->maptext[dst++] = buf[src];
				src++;
			}
			if (src >= *psize) {
				*poffs += *psize;
				*psize = reader(fileid, buf, (size_t)BUFSZ, *poffs);
				src = 0;
			} else {
				gotend = 1;
				src++;
				item->maptext[dst] = '\0';
				dst = 0;
			}
		} while (*psize && ((item->maptext[0] == '#') || !gotend));
		if (gotend) {
			pu = pg = (char*)NULL;
			/* decompose into uid, gid and sid */
			p = item->maptext;
			item->uidstr = item->maptext;
			item->gidstr = strchr(item->uidstr, ':');
			if (item->gidstr) {
				pu = item->gidstr++;
				item->sidstr = strchr(item->gidstr, ':');
				if (item->sidstr) {
					pg = item->sidstr++;
					q = strchr(item->sidstr, ':');
					if (q) *q = 0;
				}
			}
			if (pu && pg)
				*pu = *pg = '\0';
			else {
				log_early_error("Bad mapping item \"%s\"\n",
					item->maptext);
				free(item);
				item = (struct MAPLIST*)NULL;
			}
		} else {
			free(item);	/* free unused item */
			item = (struct MAPLIST*)NULL;
		}
	}
	*psrc = src;
	return (item);
}

/*
 *		Read user mapping file and split into their attribute.
 *	Parameters are kept as text in a chained list until logins
 *	are converted to uid.
 *	Returns the head of list, if any
 *
 *	If an absolute path is provided, the mapping file is assumed
 *	to be located in another mounted file system, and plain read()
 *	are used to get its contents.
 *	If a relative path is provided, the mapping file is assumed
 *	to be located on the current file system, and internal IO
 *	have to be used since we are still mounting and we have not
 *	entered the fuse loop yet.
 */

struct MAPLIST *ntfs_read_mapping(FILEREADER reader, void *fileid)
{
	char buf[BUFSZ];
	struct MAPLIST *item;
	struct MAPLIST *firstitem;
	struct MAPLIST *lastitem;
	int src;
	off_t offs;
	s64 size;

	firstitem = (struct MAPLIST*)NULL;
	lastitem = (struct MAPLIST*)NULL;
	offs = 0;
	size = reader(fileid, buf, (size_t)BUFSZ, (off_t)0);
	if (size > 0) {
		src = 0;
		do {
			item = getmappingitem(reader, fileid, &offs,
				buf, &src, &size);
			if (item) {
				item->next = (struct MAPLIST*)NULL;
				if (lastitem)
					lastitem->next = item;
				else
					firstitem = item;
				lastitem = item;
			}
		} while (item);
	}
	return (firstitem);
}

/*
 *		Free memory used to store the user mapping
 *	The only purpose is to facilitate the detection of memory leaks
 */

void ntfs_free_mapping(struct MAPPING *mapping[])
{
	struct MAPPING *user;
	struct MAPPING *group;

		/* free user mappings */
	while (mapping[MAPUSERS]) {
		user = mapping[MAPUSERS];
		/* do not free SIDs used for group mappings */
		group = mapping[MAPGROUPS];
		while (group && (group->sid != user->sid))
			group = group->next;
		if (!group)
			free(user->sid);
			/* free group list if any */
		if (user->grcnt)
			free(user->groups);
			/* unchain item and free */
		mapping[MAPUSERS] = user->next;
		free(user);
	}
		/* free group mappings */
	while (mapping[MAPGROUPS]) {
		group = mapping[MAPGROUPS];
		free(group->sid);
			/* unchain item and free */
		mapping[MAPGROUPS] = group->next;
		free(group);
	}
}


/*
 *		Build the user mapping list
 *	user identification may be given in symbolic or numeric format
 *
 *	! Note ! : does getpwnam() read /etc/passwd or some other file ?
 *		if so there is a possible recursion into fuse if this
 *		file is on NTFS, and fuse is not recursion safe.
 */

struct MAPPING *ntfs_do_user_mapping(struct MAPLIST *firstitem)
{
	struct MAPLIST *item;
	struct MAPPING *firstmapping;
	struct MAPPING *lastmapping;
	struct MAPPING *mapping;
	struct passwd *pwd;
	SID *sid;
	int uid;

	firstmapping = (struct MAPPING*)NULL;
	lastmapping = (struct MAPPING*)NULL;
	for (item = firstitem; item; item = item->next) {
		if ((item->uidstr[0] >= '0') && (item->uidstr[0] <= '9'))
			uid = atoi(item->uidstr);
		else {
			uid = 0;
			if (item->uidstr[0]) {
				pwd = getpwnam(item->uidstr);
				if (pwd)
					uid = pwd->pw_uid;
				else
					log_early_error("Invalid user \"%s\"\n",
						item->uidstr);
			}
		}
			/*
			 * Records with no uid and no gid are inserted
			 * to define the implicit mapping pattern
			 */
		if (uid
		   || (!item->uidstr[0] && !item->gidstr[0])) {
			sid = encodesid(item->sidstr);
			if (sid && !item->uidstr[0] && !item->gidstr[0]
			    && !ntfs_valid_pattern(sid)) {
				ntfs_log_error("Bad implicit SID pattern %s\n",
					item->sidstr);
				sid = (SID*)NULL;
				}
			if (sid) {
				mapping =
				    (struct MAPPING*)
				    ntfs_malloc(sizeof(struct MAPPING));
				if (mapping) {
					mapping->sid = sid;
					mapping->xid = uid;
					mapping->grcnt = 0;
					mapping->next = (struct MAPPING*)NULL;
					if (lastmapping)
						lastmapping->next = mapping;
					else
						firstmapping = mapping;
					lastmapping = mapping;
				}
			}
		}
	}
	return (firstmapping);
}

/*
 *		Build the group mapping list
 *	group identification may be given in symbolic or numeric format
 *
 *	gid not associated to a uid are processed first in order
 *	to favour real groups
 *
 *	! Note ! : does getgrnam() read /etc/group or some other file ?
 *		if so there is a possible recursion into fuse if this
 *		file is on NTFS, and fuse is not recursion safe.
 */

struct MAPPING *ntfs_do_group_mapping(struct MAPLIST *firstitem)
{
	struct MAPLIST *item;
	struct MAPPING *firstmapping;
	struct MAPPING *lastmapping;
	struct MAPPING *mapping;
	struct group *grp;
	BOOL secondstep;
	BOOL ok;
	int step;
	SID *sid;
	int gid;

	firstmapping = (struct MAPPING*)NULL;
	lastmapping = (struct MAPPING*)NULL;
	for (step=1; step<=2; step++) {
		for (item = firstitem; item; item = item->next) {
			secondstep = (item->uidstr[0] != '\0')
				|| !item->gidstr[0];
			ok = (step == 1 ? !secondstep : secondstep);
			if ((item->gidstr[0] >= '0')
			     && (item->gidstr[0] <= '9'))
				gid = atoi(item->gidstr);
			else {
				gid = 0;
				if (item->gidstr[0]) {
					grp = getgrnam(item->gidstr);
					if (grp)
						gid = grp->gr_gid;
					else
						log_early_error("Invalid group \"%s\"\n",
							item->gidstr);
				}
			}
			/*
			 * Records with no uid and no gid are inserted in the
			 * second step to define the implicit mapping pattern
			 */
			if (ok
			    && (gid
				 || (!item->uidstr[0] && !item->gidstr[0]))) {
				sid = encodesid(item->sidstr);
				if (sid && !item->uidstr[0] && !item->gidstr[0]
				    && !ntfs_valid_pattern(sid)) {
					/* error already logged */
					sid = (SID*)NULL;
					}
				if (sid) {
					mapping = (struct MAPPING*)
					    ntfs_malloc(sizeof(struct MAPPING));
					if (mapping) {
						mapping->sid = sid;
						mapping->xid = gid;
						mapping->grcnt = 0;
						mapping->next = (struct MAPPING*)NULL;
						if (lastmapping)
							lastmapping->next = mapping;
						else
							firstmapping = mapping;
						lastmapping = mapping;
					}
				}
			}
		}
	}
	return (firstmapping);
}
