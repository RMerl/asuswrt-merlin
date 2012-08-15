/*
 * security.h - Exports for handling security/ACLs in NTFS.  Part of the
 *		Linux-NTFS project.
 *
 * Copyright (c) 2004 Anton Altaparmakov
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_SECURITY_H
#define _NTFS_SECURITY_H

#include "types.h"
#include "layout.h"

extern const GUID *const zero_guid;

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

#endif /* defined _NTFS_SECURITY_H */
