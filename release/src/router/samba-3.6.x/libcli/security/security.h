/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2006

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

#ifndef _LIBCLI_SECURITY_SECURITY_H_
#define _LIBCLI_SECURITY_SECURITY_H_

#include "librpc/gen_ndr/security.h"

#define PRIMARY_USER_SID_INDEX 0
#define PRIMARY_GROUP_SID_INDEX 1

/* File Specific access rights */
#define FILE_READ_DATA        SEC_FILE_READ_DATA
#define FILE_WRITE_DATA       SEC_FILE_WRITE_DATA
#define FILE_APPEND_DATA      SEC_FILE_APPEND_DATA
#define FILE_READ_EA          SEC_FILE_READ_EA /* File and directory */
#define FILE_WRITE_EA         SEC_FILE_WRITE_EA /* File and directory */
#define FILE_EXECUTE          SEC_FILE_EXECUTE
#define FILE_READ_ATTRIBUTES  SEC_FILE_READ_ATTRIBUTE
#define FILE_WRITE_ATTRIBUTES SEC_FILE_WRITE_ATTRIBUTE

#define FILE_ALL_ACCESS       SEC_FILE_ALL

/* Directory specific access rights */
#define FILE_LIST_DIRECTORY   SEC_DIR_LIST
#define FILE_ADD_FILE         SEC_DIR_ADD_FILE
#define FILE_ADD_SUBDIRECTORY SEC_DIR_ADD_SUBDIR
#define FILE_TRAVERSE         SEC_DIR_TRAVERSE
#define FILE_DELETE_CHILD     SEC_DIR_DELETE_CHILD

/* Generic access masks & rights. */
#define DELETE_ACCESS        SEC_STD_DELETE       /* (1L<<16) */
#define READ_CONTROL_ACCESS  SEC_STD_READ_CONTROL /* (1L<<17) */
#define WRITE_DAC_ACCESS     SEC_STD_WRITE_DAC    /* (1L<<18) */
#define WRITE_OWNER_ACCESS   SEC_STD_WRITE_OWNER  /* (1L<<19) */
#define SYNCHRONIZE_ACCESS   SEC_STD_SYNCHRONIZE /* (1L<<20) */

#define SYSTEM_SECURITY_ACCESS SEC_FLAG_SYSTEM_SECURITY /* (1L<<24) */
#define MAXIMUM_ALLOWED_ACCESS SEC_FLAG_MAXIMUM_ALLOWED /* (1L<<25) */
#define GENERIC_ALL_ACCESS     SEC_GENERIC_ALL          /* (1<<28) */
#define GENERIC_EXECUTE_ACCESS SEC_GENERIC_EXECUTE      /* (1<<29) */
#define GENERIC_WRITE_ACCESS   SEC_GENERIC_WRITE        /* (1<<30) */
#define GENERIC_READ_ACCESS    ((unsigned)SEC_GENERIC_READ) /* (((unsigned)1)<<31) */

/* Mapping of generic access rights for files to specific rights. */

/* This maps to 0x1F01FF */
#define FILE_GENERIC_ALL (STANDARD_RIGHTS_REQUIRED_ACCESS|\
			  SEC_STD_SYNCHRONIZE|\
			  FILE_ALL_ACCESS)

/* This maps to 0x120089 */
#define FILE_GENERIC_READ (STANDARD_RIGHTS_READ_ACCESS|\
			   FILE_READ_DATA|\
			   FILE_READ_ATTRIBUTES|\
			   FILE_READ_EA|\
			   SYNCHRONIZE_ACCESS)

/* This maps to 0x120116 */
#define FILE_GENERIC_WRITE (SEC_STD_READ_CONTROL|\
			    FILE_WRITE_DATA|\
			    FILE_WRITE_ATTRIBUTES|\
			    FILE_WRITE_EA|\
			    FILE_APPEND_DATA|\
			    SYNCHRONIZE_ACCESS)

#define FILE_GENERIC_EXECUTE (STANDARD_RIGHTS_EXECUTE_ACCESS|\
			      FILE_READ_ATTRIBUTES|\
			      FILE_EXECUTE|\
			      SYNCHRONIZE_ACCESS)

/* Share specific rights. */
#define SHARE_ALL_ACCESS      FILE_GENERIC_ALL
#define SHARE_READ_ONLY       (FILE_GENERIC_READ|FILE_EXECUTE)

struct object_tree {
	uint32_t remaining_access;
	struct GUID guid;
	int num_of_children;
	struct object_tree *children;
};

/* Moved the dom_sid functions to the top level dir with manual proto header */
#include "libcli/security/dom_sid.h"
#include "libcli/security/secace.h"
#include "libcli/security/secacl.h"
#include "libcli/security/security_descriptor.h"
#include "libcli/security/security_token.h"
#include "libcli/security/sddl.h"
#include "libcli/security/privileges.h"
#include "libcli/security/access_check.h"
#include "libcli/security/session.h"
#include "libcli/security/display_sec.h"

#endif
