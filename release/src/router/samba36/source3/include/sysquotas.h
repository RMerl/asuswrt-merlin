/* 
    Unix SMB/CIFS implementation.
    SYS QUOTA code constants
    Copyright (C) Stefan (metze) Metzmacher	2003
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
 
#ifndef _SYSQUOTAS_H
#define _SYSQUOTAS_H
 
#ifdef HAVE_SYS_QUOTAS

#if defined(HAVE_MNTENT_H)&&defined(HAVE_SETMNTENT)&&defined(HAVE_GETMNTENT)&&defined(HAVE_ENDMNTENT)
#include <mntent.h>
#define HAVE_MNTENT 1
/*#endif defined(HAVE_MNTENT_H)&&defined(HAVE_SETMNTENT)&&defined(HAVE_GETMNTENT)&&defined(HAVE_ENDMNTENT) */
#elif defined(HAVE_DEVNM_H)&&defined(HAVE_DEVNM)
#include <devnm.h>
#endif /* defined(HAVE_DEVNM_H)&&defined(HAVE_DEVNM) */

#endif /* HAVE_SYS_QUOTAS */


/**************************************************
 Some stuff for the sys_quota api.
 **************************************************/ 

#define SMB_QUOTAS_NO_LIMIT	((uint64_t)(0))
#define SMB_QUOTAS_NO_SPACE	((uint64_t)(1))

#define SMB_QUOTAS_SET_NO_LIMIT(dp) \
{\
	(dp)->softlimit = SMB_QUOTAS_NO_LIMIT;\
	(dp)->hardlimit = SMB_QUOTAS_NO_LIMIT;\
	(dp)->isoftlimit = SMB_QUOTAS_NO_LIMIT;\
	(dp)->ihardlimit = SMB_QUOTAS_NO_LIMIT;\
}

#define SMB_QUOTAS_SET_NO_SPACE(dp) \
{\
	(dp)->softlimit = SMB_QUOTAS_NO_SPACE;\
	(dp)->hardlimit = SMB_QUOTAS_NO_SPACE;\
	(dp)->isoftlimit = SMB_QUOTAS_NO_SPACE;\
	(dp)->ihardlimit = SMB_QUOTAS_NO_SPACE;\
}

typedef struct _SMB_DISK_QUOTA {
	enum SMB_QUOTA_TYPE qtype;
	uint64_t bsize;
	uint64_t hardlimit; /* In bsize units. */
	uint64_t softlimit; /* In bsize units. */
	uint64_t curblocks; /* In bsize units. */
	uint64_t ihardlimit; /* inode hard limit. */
	uint64_t isoftlimit; /* inode soft limit. */
	uint64_t curinodes; /* Current used inodes. */
	uint32_t       qflags;
} SMB_DISK_QUOTA;

#ifndef QUOTABLOCK_SIZE
#define QUOTABLOCK_SIZE 1024
#endif

#endif /*_SYSQUOTAS_H */
