/*
 * Unix SMB/CIFS implementation.
 * OneFS vfs module configuration and defaults
 *
 * Copyright (C) Steven Danneman, 2008
 * Copyright (C) Tim Prouty, 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ONEFS_CONFIG_H
#define _ONEFS_CONFIG_H

/**
* Specifies when ACLs presented to Windows should be canonicalized
* into the ordering which Explorer expects.
*/
enum onefs_acl_wire_format
{
	ACL_FORMAT_RAW, /**< Never canonicalize */
	ACL_FORMAT_WINDOWS_SD, /**< Only canonicalize synthetic ACLs */
	ACL_FORMAT_ALWAYS /**< Always canonicalize */
};

#define PARM_ONEFS_TYPE "onefs"
#define PARM_ACL_WIRE_FORMAT "acl wire format"
#define PARM_ACL_WIRE_FORMAT_DEFAULT ACL_FORMAT_WINDOWS_SD
#define PARM_ALLOW_EXECUTE_ALWAYS "allow execute always"
#define PARM_ALLOW_EXECUTE_ALWAYS_DEFAULT false
#define PARM_ATIME_NOW		"atime now files"
#define PARM_ATIME_NOW_DEFAULT  NULL
#define PARM_ATIME_STATIC	"atime static files"
#define PARM_ATIME_STATIC_DEFAULT NULL
#define PARM_ATIME_SLOP		"atime now slop"
#define PARM_ATIME_SLOP_DEFAULT	 0
#define PARM_ATOMIC_SENDFILE "atomic sendfile"
#define PARM_ATOMIC_SENDFILE_DEFAULT true
#define PARM_CREATOR_OWNER_GETS_FULL_CONTROL "creator owner gets full control"
#define PARM_CREATOR_OWNER_GETS_FULL_CONTROL_DEFAULT true
#define PARM_CTIME_NOW		"ctime now files"
#define PARM_CTIME_NOW_DEFAULT  NULL
#define PARM_CTIME_SLOP		"ctime now slop"
#define PARM_CTIME_SLOP_DEFAULT	0
#define PARM_DOT_SNAP_CHILD_ACCESSIBLE "dot snap child accessible"
#define PARM_DOT_SNAP_CHILD_ACCESSIBLE_DEFAULT true
#define PARM_DOT_SNAP_CHILD_VISIBLE "dot snap child visible"
#define PARM_DOT_SNAP_CHILD_VISIBLE_DEFAULT false
#define PARM_DOT_SNAP_ROOT_ACCESSIBLE "dot snap root accessible"
#define PARM_DOT_SNAP_ROOT_ACCESSIBLE_DEFAULT true
#define PARM_DOT_SNAP_ROOT_VISIBLE "dot snap root visible"
#define PARM_DOT_SNAP_ROOT_VISIBLE_DEFAULT true
#define PARM_DOT_SNAP_TILDE "dot snap tilde"
#define PARM_DOT_SNAP_TILDE_DEFAULT true
#define PARM_IGNORE_SACLS "ignore sacls"
#define PARM_IGNORE_SACLS_DEFAULT false
#define PARM_IGNORE_STREAMS "ignore streams"
#define PARM_IGNORE_STREAMS_DEFAULT false
#define PARM_MTIME_NOW		"mtime now files"
#define PARM_MTIME_NOW_DEFAULT	NULL
#define PARM_MTIME_STATIC	"mtime static files"
#define PARM_MTIME_STATIC_DEFAULT NULL
#define PARM_MTIME_SLOP		"mtime now slop"
#define PARM_MTIME_SLOP_DEFAULT	0
#define PARM_USE_READDIRPLUS "use readdirplus"
#define PARM_USE_READDIRPLUS_DEFAULT true
#define PARM_SENDFILE_LARGE_READS "sendfile large reads"
#define PARM_SENDFILE_LARGE_READS_DEFAULT false
#define PARM_SENDFILE_SAFE "sendfile safe"
#define PARM_SENDFILE_SAFE_DEFAULT true
#define PARM_SIMPLE_FILE_SHARING_COMPATIBILITY_MODE "simple file sharing compatibility mode"
#define PARM_SIMPLE_FILE_SHARING_COMPATIBILITY_MODE_DEFAULT false
#define PARM_UNMAPPABLE_SIDS_DENY_EVERYONE "unmappable sids deny everyone"
#define PARM_UNMAPPABLE_SIDS_DENY_EVERYONE_DEFAULT false
#define PARM_UNMAPPABLE_SIDS_IGNORE "ignore unmappable sids"
#define PARM_UNMAPPABLE_SIDS_IGNORE_DEFAULT false
#define PARM_UNMAPPABLE_SIDS_IGNORE_LIST "unmappable sids ignore list"
#define PARM_UNMAPPABLE_SIDS_IGNORE_LIST_DEFAULT NULL

#define IS_CTIME_NOW_PATH(conn,cfg,path)  ((conn) && is_in_path((path),\
	(cfg)->ctime_now_list,(conn)->case_sensitive))
#define IS_MTIME_NOW_PATH(conn,cfg,path)  ((conn) && is_in_path((path),\
	(cfg)->mtime_now_list,(conn)->case_sensitive))
#define IS_ATIME_NOW_PATH(conn,cfg,path)  ((conn) && is_in_path((path),\
	(cfg)->atime_now_list,(conn)->case_sensitive))
#define IS_MTIME_STATIC_PATH(conn,cfg,path)  ((conn) && is_in_path((path),\
	(cfg)->mtime_static_list,(conn)->case_sensitive))
#define IS_ATIME_STATIC_PATH(conn,cfg,path)  ((conn) && is_in_path((path),\
	(cfg)->atime_static_list,(conn)->case_sensitive))

/*
 * Store some commonly evaluated parameters to avoid loadparm pain.
 */

#define ONEFS_VFS_CONFIG_INITIALIZED	0x00010000

#define ONEFS_VFS_CONFIG_FAKETIMESTAMPS	0x00000001

struct onefs_vfs_share_config
{
	uint32_t init_flags;

	/* data for fake timestamps */
	int atime_slop;
	int ctime_slop;
	int mtime_slop;

	/* Per-share list of files to fake the create time for. */
        name_compare_entry *ctime_now_list;

	/* Per-share list of files to fake the modification time for. */
	name_compare_entry *mtime_now_list;

	/* Per-share list of files to fake the access time for. */
	name_compare_entry *atime_now_list;

	/* Per-share list of files to fake the modification time for. */
	name_compare_entry *mtime_static_list;

	/* The access  time  will  equal  the  create  time.  */
	/* The  modification  time  will  equal  the  create  time.*/

	/* Per-share list of files to fake the access time for. */
	name_compare_entry *atime_static_list;
};

struct onefs_vfs_global_config
{
	uint32_t init_flags;

	/* Snapshot options */
	bool dot_snap_child_accessible;
	bool dot_snap_child_visible;
	bool dot_snap_root_accessible;
	bool dot_snap_root_visible;
	bool dot_snap_tilde;
};

int onefs_load_config(connection_struct *conn);

bool onefs_get_config(int snum, int config_type,
		      struct onefs_vfs_share_config *cfg);

void onefs_sys_config_enc(void);

void onefs_sys_config_snap_opt(struct onefs_vfs_global_config *global_config);

void onefs_sys_config_tilde(struct onefs_vfs_global_config *global_config);

#endif /* _ONEFS_CONFIG_H */
