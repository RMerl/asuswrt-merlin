/* 
 * Auditing VFS module for samba.  Log selected file operations to syslog
 * facility.
 *
 * Copyright (C) Tim Potter, 1999-2000
 * Copyright (C) Alexander Bokovoy, 2002
 * Copyright (C) John H Terpstra, 2003
 * Copyright (C) Stefan (metze) Metzmacher, 2003
 * Copyright (C) Volker Lendecke, 2004
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

/*
 * This module implements parseable logging for all Samba VFS operations.
 *
 * You use it as follows:
 *
 * [tmp]
 * path = /tmp
 * vfs objects = full_audit
 * full_audit:prefix = %u|%I
 * full_audit:success = open opendir
 * full_audit:failure = all
 *
 * vfs op can be "all" which means log all operations.
 * vfs op can be "none" which means no logging.
 *
 * This leads to syslog entries of the form:
 * smbd_audit: nobody|192.168.234.1|opendir|ok|.
 * smbd_audit: nobody|192.168.234.1|open|fail (File not found)|r|x.txt
 *
 * where "nobody" is the connected username and "192.168.234.1" is the
 * client's IP address. 
 *
 * Options:
 *
 * prefix: A macro expansion template prepended to the syslog entry.
 *
 * success: A list of VFS operations for which a successful completion should
 * be logged. Defaults to no logging at all. The special operation "all" logs
 * - you guessed it - everything.
 *
 * failure: A list of VFS operations for which failure to complete should be
 * logged. Defaults to logging everything.
 */


#include "includes.h"
#include "system/filesys.h"
#include "system/syslog.h"
#include "smbd/smbd.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "auth.h"
#include "ntioctl.h"
#include "lib/util/bitmap.h"

static int vfs_full_audit_debug_level = DBGC_VFS;

struct vfs_full_audit_private_data {
	struct bitmap *success_ops;
	struct bitmap *failure_ops;
};

#undef DBGC_CLASS
#define DBGC_CLASS vfs_full_audit_debug_level

typedef enum _vfs_op_type {
	SMB_VFS_OP_NOOP = -1,

	/* Disk operations */

	SMB_VFS_OP_CONNECT = 0,
	SMB_VFS_OP_DISCONNECT,
	SMB_VFS_OP_DISK_FREE,
	SMB_VFS_OP_GET_QUOTA,
	SMB_VFS_OP_SET_QUOTA,
	SMB_VFS_OP_GET_SHADOW_COPY_DATA,
	SMB_VFS_OP_STATVFS,
	SMB_VFS_OP_FS_CAPABILITIES,

	/* Directory operations */

	SMB_VFS_OP_OPENDIR,
	SMB_VFS_OP_FDOPENDIR,
	SMB_VFS_OP_READDIR,
	SMB_VFS_OP_SEEKDIR,
	SMB_VFS_OP_TELLDIR,
	SMB_VFS_OP_REWINDDIR,
	SMB_VFS_OP_MKDIR,
	SMB_VFS_OP_RMDIR,
	SMB_VFS_OP_CLOSEDIR,
	SMB_VFS_OP_INIT_SEARCH_OP,

	/* File operations */

	SMB_VFS_OP_OPEN,
	SMB_VFS_OP_CREATE_FILE,
	SMB_VFS_OP_CLOSE,
	SMB_VFS_OP_READ,
	SMB_VFS_OP_PREAD,
	SMB_VFS_OP_WRITE,
	SMB_VFS_OP_PWRITE,
	SMB_VFS_OP_LSEEK,
	SMB_VFS_OP_SENDFILE,
	SMB_VFS_OP_RECVFILE,
	SMB_VFS_OP_RENAME,
	SMB_VFS_OP_FSYNC,
	SMB_VFS_OP_STAT,
	SMB_VFS_OP_FSTAT,
	SMB_VFS_OP_LSTAT,
	SMB_VFS_OP_GET_ALLOC_SIZE,
	SMB_VFS_OP_UNLINK,
	SMB_VFS_OP_CHMOD,
	SMB_VFS_OP_FCHMOD,
	SMB_VFS_OP_CHOWN,
	SMB_VFS_OP_FCHOWN,
	SMB_VFS_OP_LCHOWN,
	SMB_VFS_OP_CHDIR,
	SMB_VFS_OP_GETWD,
	SMB_VFS_OP_NTIMES,
	SMB_VFS_OP_FTRUNCATE,
	SMB_VFS_OP_FALLOCATE,
	SMB_VFS_OP_LOCK,
	SMB_VFS_OP_KERNEL_FLOCK,
	SMB_VFS_OP_LINUX_SETLEASE,
	SMB_VFS_OP_GETLOCK,
	SMB_VFS_OP_SYMLINK,
	SMB_VFS_OP_READLINK,
	SMB_VFS_OP_LINK,
	SMB_VFS_OP_MKNOD,
	SMB_VFS_OP_REALPATH,
	SMB_VFS_OP_NOTIFY_WATCH,
	SMB_VFS_OP_CHFLAGS,
	SMB_VFS_OP_FILE_ID_CREATE,
	SMB_VFS_OP_STREAMINFO,
	SMB_VFS_OP_GET_REAL_FILENAME,
	SMB_VFS_OP_CONNECTPATH,
	SMB_VFS_OP_BRL_LOCK_WINDOWS,
	SMB_VFS_OP_BRL_UNLOCK_WINDOWS,
	SMB_VFS_OP_BRL_CANCEL_WINDOWS,
	SMB_VFS_OP_STRICT_LOCK,
	SMB_VFS_OP_STRICT_UNLOCK,
	SMB_VFS_OP_TRANSLATE_NAME,

	/* NT ACL operations. */

	SMB_VFS_OP_FGET_NT_ACL,
	SMB_VFS_OP_GET_NT_ACL,
	SMB_VFS_OP_FSET_NT_ACL,

	/* POSIX ACL operations. */

	SMB_VFS_OP_CHMOD_ACL,
	SMB_VFS_OP_FCHMOD_ACL,

	SMB_VFS_OP_SYS_ACL_GET_ENTRY,
	SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE,
	SMB_VFS_OP_SYS_ACL_GET_PERMSET,
	SMB_VFS_OP_SYS_ACL_GET_QUALIFIER,
	SMB_VFS_OP_SYS_ACL_GET_FILE,
	SMB_VFS_OP_SYS_ACL_GET_FD,
	SMB_VFS_OP_SYS_ACL_CLEAR_PERMS,
	SMB_VFS_OP_SYS_ACL_ADD_PERM,
	SMB_VFS_OP_SYS_ACL_TO_TEXT,
	SMB_VFS_OP_SYS_ACL_INIT,
	SMB_VFS_OP_SYS_ACL_CREATE_ENTRY,
	SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE,
	SMB_VFS_OP_SYS_ACL_SET_QUALIFIER,
	SMB_VFS_OP_SYS_ACL_SET_PERMSET,
	SMB_VFS_OP_SYS_ACL_VALID,
	SMB_VFS_OP_SYS_ACL_SET_FILE,
	SMB_VFS_OP_SYS_ACL_SET_FD,
	SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
	SMB_VFS_OP_SYS_ACL_GET_PERM,
	SMB_VFS_OP_SYS_ACL_FREE_TEXT,
	SMB_VFS_OP_SYS_ACL_FREE_ACL,
	SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER,

	/* EA operations. */
	SMB_VFS_OP_GETXATTR,
	SMB_VFS_OP_LGETXATTR,
	SMB_VFS_OP_FGETXATTR,
	SMB_VFS_OP_LISTXATTR,
	SMB_VFS_OP_LLISTXATTR,
	SMB_VFS_OP_FLISTXATTR,
	SMB_VFS_OP_REMOVEXATTR,
	SMB_VFS_OP_LREMOVEXATTR,
	SMB_VFS_OP_FREMOVEXATTR,
	SMB_VFS_OP_SETXATTR,
	SMB_VFS_OP_LSETXATTR,
	SMB_VFS_OP_FSETXATTR,

	/* aio operations */
	SMB_VFS_OP_AIO_READ,
	SMB_VFS_OP_AIO_WRITE,
	SMB_VFS_OP_AIO_RETURN,
	SMB_VFS_OP_AIO_CANCEL,
	SMB_VFS_OP_AIO_ERROR,
	SMB_VFS_OP_AIO_FSYNC,
	SMB_VFS_OP_AIO_SUSPEND,
        SMB_VFS_OP_AIO_FORCE,

	/* offline operations */
	SMB_VFS_OP_IS_OFFLINE,
	SMB_VFS_OP_SET_OFFLINE,

	/* This should always be last enum value */

	SMB_VFS_OP_LAST
} vfs_op_type;

/* The following array *must* be in the same order as defined in vfs.h */

static struct {
	vfs_op_type type;
	const char *name;
} vfs_op_names[] = {
	{ SMB_VFS_OP_CONNECT,	"connect" },
	{ SMB_VFS_OP_DISCONNECT,	"disconnect" },
	{ SMB_VFS_OP_DISK_FREE,	"disk_free" },
	{ SMB_VFS_OP_GET_QUOTA,	"get_quota" },
	{ SMB_VFS_OP_SET_QUOTA,	"set_quota" },
	{ SMB_VFS_OP_GET_SHADOW_COPY_DATA,	"get_shadow_copy_data" },
	{ SMB_VFS_OP_STATVFS,	"statvfs" },
	{ SMB_VFS_OP_FS_CAPABILITIES,	"fs_capabilities" },
	{ SMB_VFS_OP_OPENDIR,	"opendir" },
	{ SMB_VFS_OP_FDOPENDIR,	"fdopendir" },
	{ SMB_VFS_OP_READDIR,	"readdir" },
	{ SMB_VFS_OP_SEEKDIR,   "seekdir" },
	{ SMB_VFS_OP_TELLDIR,   "telldir" },
	{ SMB_VFS_OP_REWINDDIR, "rewinddir" },
	{ SMB_VFS_OP_MKDIR,	"mkdir" },
	{ SMB_VFS_OP_RMDIR,	"rmdir" },
	{ SMB_VFS_OP_CLOSEDIR,	"closedir" },
	{ SMB_VFS_OP_INIT_SEARCH_OP, "init_search_op" },
	{ SMB_VFS_OP_OPEN,	"open" },
	{ SMB_VFS_OP_CREATE_FILE, "create_file" },
	{ SMB_VFS_OP_CLOSE,	"close" },
	{ SMB_VFS_OP_READ,	"read" },
	{ SMB_VFS_OP_PREAD,	"pread" },
	{ SMB_VFS_OP_WRITE,	"write" },
	{ SMB_VFS_OP_PWRITE,	"pwrite" },
	{ SMB_VFS_OP_LSEEK,	"lseek" },
	{ SMB_VFS_OP_SENDFILE,	"sendfile" },
	{ SMB_VFS_OP_RECVFILE,  "recvfile" },
	{ SMB_VFS_OP_RENAME,	"rename" },
	{ SMB_VFS_OP_FSYNC,	"fsync" },
	{ SMB_VFS_OP_STAT,	"stat" },
	{ SMB_VFS_OP_FSTAT,	"fstat" },
	{ SMB_VFS_OP_LSTAT,	"lstat" },
	{ SMB_VFS_OP_GET_ALLOC_SIZE,	"get_alloc_size" },
	{ SMB_VFS_OP_UNLINK,	"unlink" },
	{ SMB_VFS_OP_CHMOD,	"chmod" },
	{ SMB_VFS_OP_FCHMOD,	"fchmod" },
	{ SMB_VFS_OP_CHOWN,	"chown" },
	{ SMB_VFS_OP_FCHOWN,	"fchown" },
	{ SMB_VFS_OP_LCHOWN,	"lchown" },
	{ SMB_VFS_OP_CHDIR,	"chdir" },
	{ SMB_VFS_OP_GETWD,	"getwd" },
	{ SMB_VFS_OP_NTIMES,	"ntimes" },
	{ SMB_VFS_OP_FTRUNCATE,	"ftruncate" },
	{ SMB_VFS_OP_FALLOCATE,"fallocate" },
	{ SMB_VFS_OP_LOCK,	"lock" },
	{ SMB_VFS_OP_KERNEL_FLOCK,	"kernel_flock" },
	{ SMB_VFS_OP_LINUX_SETLEASE, "linux_setlease" },
	{ SMB_VFS_OP_GETLOCK,	"getlock" },
	{ SMB_VFS_OP_SYMLINK,	"symlink" },
	{ SMB_VFS_OP_READLINK,	"readlink" },
	{ SMB_VFS_OP_LINK,	"link" },
	{ SMB_VFS_OP_MKNOD,	"mknod" },
	{ SMB_VFS_OP_REALPATH,	"realpath" },
	{ SMB_VFS_OP_NOTIFY_WATCH, "notify_watch" },
	{ SMB_VFS_OP_CHFLAGS,	"chflags" },
	{ SMB_VFS_OP_FILE_ID_CREATE,	"file_id_create" },
	{ SMB_VFS_OP_STREAMINFO,	"streaminfo" },
	{ SMB_VFS_OP_GET_REAL_FILENAME, "get_real_filename" },
	{ SMB_VFS_OP_CONNECTPATH,	"connectpath" },
	{ SMB_VFS_OP_BRL_LOCK_WINDOWS,  "brl_lock_windows" },
	{ SMB_VFS_OP_BRL_UNLOCK_WINDOWS, "brl_unlock_windows" },
	{ SMB_VFS_OP_BRL_CANCEL_WINDOWS, "brl_cancel_windows" },
	{ SMB_VFS_OP_STRICT_LOCK, "strict_lock" },
	{ SMB_VFS_OP_STRICT_UNLOCK, "strict_unlock" },
	{ SMB_VFS_OP_TRANSLATE_NAME,	"translate_name" },
	{ SMB_VFS_OP_FGET_NT_ACL,	"fget_nt_acl" },
	{ SMB_VFS_OP_GET_NT_ACL,	"get_nt_acl" },
	{ SMB_VFS_OP_FSET_NT_ACL,	"fset_nt_acl" },
	{ SMB_VFS_OP_CHMOD_ACL,	"chmod_acl" },
	{ SMB_VFS_OP_FCHMOD_ACL,	"fchmod_acl" },
	{ SMB_VFS_OP_SYS_ACL_GET_ENTRY,	"sys_acl_get_entry" },
	{ SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE,	"sys_acl_get_tag_type" },
	{ SMB_VFS_OP_SYS_ACL_GET_PERMSET,	"sys_acl_get_permset" },
	{ SMB_VFS_OP_SYS_ACL_GET_QUALIFIER,	"sys_acl_get_qualifier" },
	{ SMB_VFS_OP_SYS_ACL_GET_FILE,	"sys_acl_get_file" },
	{ SMB_VFS_OP_SYS_ACL_GET_FD,	"sys_acl_get_fd" },
	{ SMB_VFS_OP_SYS_ACL_CLEAR_PERMS,	"sys_acl_clear_perms" },
	{ SMB_VFS_OP_SYS_ACL_ADD_PERM,	"sys_acl_add_perm" },
	{ SMB_VFS_OP_SYS_ACL_TO_TEXT,	"sys_acl_to_text" },
	{ SMB_VFS_OP_SYS_ACL_INIT,	"sys_acl_init" },
	{ SMB_VFS_OP_SYS_ACL_CREATE_ENTRY,	"sys_acl_create_entry" },
	{ SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE,	"sys_acl_set_tag_type" },
	{ SMB_VFS_OP_SYS_ACL_SET_QUALIFIER,	"sys_acl_set_qualifier" },
	{ SMB_VFS_OP_SYS_ACL_SET_PERMSET,	"sys_acl_set_permset" },
	{ SMB_VFS_OP_SYS_ACL_VALID,	"sys_acl_valid" },
	{ SMB_VFS_OP_SYS_ACL_SET_FILE,	"sys_acl_set_file" },
	{ SMB_VFS_OP_SYS_ACL_SET_FD,	"sys_acl_set_fd" },
	{ SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,	"sys_acl_delete_def_file" },
	{ SMB_VFS_OP_SYS_ACL_GET_PERM,	"sys_acl_get_perm" },
	{ SMB_VFS_OP_SYS_ACL_FREE_TEXT,	"sys_acl_free_text" },
	{ SMB_VFS_OP_SYS_ACL_FREE_ACL,	"sys_acl_free_acl" },
	{ SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER,	"sys_acl_free_qualifier" },
	{ SMB_VFS_OP_GETXATTR,	"getxattr" },
	{ SMB_VFS_OP_LGETXATTR,	"lgetxattr" },
	{ SMB_VFS_OP_FGETXATTR,	"fgetxattr" },
	{ SMB_VFS_OP_LISTXATTR,	"listxattr" },
	{ SMB_VFS_OP_LLISTXATTR,	"llistxattr" },
	{ SMB_VFS_OP_FLISTXATTR,	"flistxattr" },
	{ SMB_VFS_OP_REMOVEXATTR,	"removexattr" },
	{ SMB_VFS_OP_LREMOVEXATTR,	"lremovexattr" },
	{ SMB_VFS_OP_FREMOVEXATTR,	"fremovexattr" },
	{ SMB_VFS_OP_SETXATTR,	"setxattr" },
	{ SMB_VFS_OP_LSETXATTR,	"lsetxattr" },
	{ SMB_VFS_OP_FSETXATTR,	"fsetxattr" },
	{ SMB_VFS_OP_AIO_READ,	"aio_read" },
	{ SMB_VFS_OP_AIO_WRITE,	"aio_write" },
	{ SMB_VFS_OP_AIO_RETURN,"aio_return" },
	{ SMB_VFS_OP_AIO_CANCEL,"aio_cancel" },
	{ SMB_VFS_OP_AIO_ERROR,	"aio_error" },
	{ SMB_VFS_OP_AIO_FSYNC,	"aio_fsync" },
	{ SMB_VFS_OP_AIO_SUSPEND,"aio_suspend" },
	{ SMB_VFS_OP_AIO_FORCE, "aio_force" },
	{ SMB_VFS_OP_IS_OFFLINE, "is_offline" },
	{ SMB_VFS_OP_SET_OFFLINE, "set_offline" },
	{ SMB_VFS_OP_LAST, NULL }
};

static int audit_syslog_facility(vfs_handle_struct *handle)
{
	static const struct enum_list enum_log_facilities[] = {
		{ LOG_USER, "USER" },
		{ LOG_LOCAL0, "LOCAL0" },
		{ LOG_LOCAL1, "LOCAL1" },
		{ LOG_LOCAL2, "LOCAL2" },
		{ LOG_LOCAL3, "LOCAL3" },
		{ LOG_LOCAL4, "LOCAL4" },
		{ LOG_LOCAL5, "LOCAL5" },
		{ LOG_LOCAL6, "LOCAL6" },
		{ LOG_LOCAL7, "LOCAL7" }
	};

	int facility;

	facility = lp_parm_enum(SNUM(handle->conn), "full_audit", "facility", enum_log_facilities, LOG_USER);

	return facility;
}

static int audit_syslog_priority(vfs_handle_struct *handle)
{
	static const struct enum_list enum_log_priorities[] = {
		{ LOG_EMERG, "EMERG" },
		{ LOG_ALERT, "ALERT" },
		{ LOG_CRIT, "CRIT" },
		{ LOG_ERR, "ERR" },
		{ LOG_WARNING, "WARNING" },
		{ LOG_NOTICE, "NOTICE" },
		{ LOG_INFO, "INFO" },
		{ LOG_DEBUG, "DEBUG" }
	};

	int priority;

	priority = lp_parm_enum(SNUM(handle->conn), "full_audit", "priority",
				enum_log_priorities, LOG_NOTICE);
	if (priority == -1) {
		priority = LOG_WARNING;
	}

	return priority;
}

static char *audit_prefix(TALLOC_CTX *ctx, connection_struct *conn)
{
	char *prefix = NULL;
	char *result;

	prefix = talloc_strdup(ctx,
			lp_parm_const_string(SNUM(conn), "full_audit",
					     "prefix", "%u|%I"));
	if (!prefix) {
		return NULL;
	}
	result = talloc_sub_advanced(ctx,
			lp_servicename(SNUM(conn)),
			conn->session_info->unix_name,
			conn->connectpath,
			conn->session_info->utok.gid,
			conn->session_info->sanitized_username,
			conn->session_info->info3->base.domain.string,
			prefix);
	TALLOC_FREE(prefix);
	return result;
}

static bool log_success(vfs_handle_struct *handle, vfs_op_type op)
{
	struct vfs_full_audit_private_data *pd = NULL;

	SMB_VFS_HANDLE_GET_DATA(handle, pd,
		struct vfs_full_audit_private_data,
		return True);

	if (pd->success_ops == NULL) {
		return True;
	}

	return bitmap_query(pd->success_ops, op);
}

static bool log_failure(vfs_handle_struct *handle, vfs_op_type op)
{
	struct vfs_full_audit_private_data *pd = NULL;

	SMB_VFS_HANDLE_GET_DATA(handle, pd,
		struct vfs_full_audit_private_data,
		return True);

	if (pd->failure_ops == NULL)
		return True;

	return bitmap_query(pd->failure_ops, op);
}

static struct bitmap *init_bitmap(TALLOC_CTX *mem_ctx, const char **ops)
{
	struct bitmap *bm;

	if (ops == NULL) {
		return NULL;
	}

	bm = bitmap_talloc(mem_ctx, SMB_VFS_OP_LAST);
	if (bm == NULL) {
		DEBUG(0, ("Could not alloc bitmap -- "
			  "defaulting to logging everything\n"));
		return NULL;
	}

	for (; *ops != NULL; ops += 1) {
		int i;
		bool neg = false;
		const char *op;

		if (strequal(*ops, "all")) {
			for (i=0; i<SMB_VFS_OP_LAST; i++) {
				bitmap_set(bm, i);
			}
			continue;
		}

		if (strequal(*ops, "none")) {
			break;
		}

		op = ops[0];
		if (op[0] == '!') {
			neg = true;
			op += 1;
		}

		for (i=0; i<SMB_VFS_OP_LAST; i++) {
			if (vfs_op_names[i].name == NULL) {
				smb_panic("vfs_full_audit.c: name table not "
					  "in sync with vfs.h\n");
			}
			if (strequal(op, vfs_op_names[i].name)) {
				if (neg) {
					bitmap_clear(bm, i);
				} else {
					bitmap_set(bm, i);
				}
				break;
			}
		}
		if (i == SMB_VFS_OP_LAST) {
			DEBUG(0, ("Could not find opname %s, logging all\n",
				  *ops));
			TALLOC_FREE(bm);
			return NULL;
		}
	}
	return bm;
}

static const char *audit_opname(vfs_op_type op)
{
	if (op >= SMB_VFS_OP_LAST)
		return "INVALID VFS OP";
	return vfs_op_names[op].name;
}

static TALLOC_CTX *tmp_do_log_ctx;
/*
 * Get us a temporary talloc context usable just for DEBUG arguments
 */
static TALLOC_CTX *do_log_ctx(void)
{
        if (tmp_do_log_ctx == NULL) {
                tmp_do_log_ctx = talloc_named_const(NULL, 0, "do_log_ctx");
        }
        return tmp_do_log_ctx;
}

static void do_log(vfs_op_type op, bool success, vfs_handle_struct *handle,
		   const char *format, ...)
{
	fstring err_msg;
	char *audit_pre = NULL;
	va_list ap;
	char *op_msg = NULL;
	int priority;

	if (success && (!log_success(handle, op)))
		goto out;

	if (!success && (!log_failure(handle, op)))
		goto out;

	if (success)
		fstrcpy(err_msg, "ok");
	else
		fstr_sprintf(err_msg, "fail (%s)", strerror(errno));

	va_start(ap, format);
	op_msg = talloc_vasprintf(talloc_tos(), format, ap);
	va_end(ap);

	if (!op_msg) {
		goto out;
	}

	/*
	 * Specify the facility to interoperate with other syslog callers
	 * (smbd for example).
	 */
	priority = audit_syslog_priority(handle) |
	    audit_syslog_facility(handle);

	audit_pre = audit_prefix(talloc_tos(), handle->conn);
	syslog(priority, "%s|%s|%s|%s\n",
		audit_pre ? audit_pre : "",
		audit_opname(op), err_msg, op_msg);

 out:
	TALLOC_FREE(audit_pre);
	TALLOC_FREE(op_msg);
	TALLOC_FREE(tmp_do_log_ctx);

	return;
}

/**
 * Return a string using the do_log_ctx()
 */
static const char *smb_fname_str_do_log(const struct smb_filename *smb_fname)
{
	char *fname = NULL;
	NTSTATUS status;

	if (smb_fname == NULL) {
		return "";
	}
	status = get_full_smb_filename(do_log_ctx(), smb_fname, &fname);
	if (!NT_STATUS_IS_OK(status)) {
		return "";
	}
	return fname;
}

/**
 * Return an fsp debug string using the do_log_ctx()
 */
static const char *fsp_str_do_log(const struct files_struct *fsp)
{
	return smb_fname_str_do_log(fsp->fsp_name);
}

/* Implementation of vfs_ops.  Pass everything on to the default
   operation but log event first. */

static int smb_full_audit_connect(vfs_handle_struct *handle,
			 const char *svc, const char *user)
{
	int result;
	struct vfs_full_audit_private_data *pd = NULL;

	result = SMB_VFS_NEXT_CONNECT(handle, svc, user);
	if (result < 0) {
		return result;
	}

	pd = TALLOC_ZERO_P(handle, struct vfs_full_audit_private_data);
	if (!pd) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		return -1;
	}

#ifndef WITH_SYSLOG
	openlog("smbd_audit", 0, audit_syslog_facility(handle));
#endif

	pd->success_ops = init_bitmap(
		pd, lp_parm_string_list(SNUM(handle->conn), "full_audit",
					"success", NULL));
	pd->failure_ops = init_bitmap(
		pd, lp_parm_string_list(SNUM(handle->conn), "full_audit",
					"failure", NULL));

	/* Store the private data. */
	SMB_VFS_HANDLE_SET_DATA(handle, pd, NULL,
				struct vfs_full_audit_private_data, return -1);

	do_log(SMB_VFS_OP_CONNECT, True, handle,
	       "%s", svc);

	return 0;
}

static void smb_full_audit_disconnect(vfs_handle_struct *handle)
{
	SMB_VFS_NEXT_DISCONNECT(handle);

	do_log(SMB_VFS_OP_DISCONNECT, True, handle,
	       "%s", lp_servicename(SNUM(handle->conn)));

	/* The bitmaps will be disconnected when the private
	   data is deleted. */

	return;
}

static uint64_t smb_full_audit_disk_free(vfs_handle_struct *handle,
				    const char *path,
				    bool small_query, uint64_t *bsize, 
				    uint64_t *dfree, uint64_t *dsize)
{
	uint64_t result;

	result = SMB_VFS_NEXT_DISK_FREE(handle, path, small_query, bsize,
					dfree, dsize);

	/* Don't have a reasonable notion of failure here */

	do_log(SMB_VFS_OP_DISK_FREE, True, handle, "%s", path);

	return result;
}

static int smb_full_audit_get_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt)
{
	int result;

	result = SMB_VFS_NEXT_GET_QUOTA(handle, qtype, id, qt);

	do_log(SMB_VFS_OP_GET_QUOTA, (result >= 0), handle, "");

	return result;
}

	
static int smb_full_audit_set_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt)
{
	int result;

	result = SMB_VFS_NEXT_SET_QUOTA(handle, qtype, id, qt);

	do_log(SMB_VFS_OP_SET_QUOTA, (result >= 0), handle, "");

	return result;
}

static int smb_full_audit_get_shadow_copy_data(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct shadow_copy_data *shadow_copy_data,
				bool labels)
{
	int result;

	result = SMB_VFS_NEXT_GET_SHADOW_COPY_DATA(handle, fsp, shadow_copy_data, labels);

	do_log(SMB_VFS_OP_GET_SHADOW_COPY_DATA, (result >= 0), handle, "");

	return result;
}

static int smb_full_audit_statvfs(struct vfs_handle_struct *handle,
				const char *path,
				struct vfs_statvfs_struct *statbuf)
{
	int result;

	result = SMB_VFS_NEXT_STATVFS(handle, path, statbuf);

	do_log(SMB_VFS_OP_STATVFS, (result >= 0), handle, "");

	return result;
}

static uint32_t smb_full_audit_fs_capabilities(struct vfs_handle_struct *handle, enum timestamp_set_resolution *p_ts_res)
{
	int result;

	result = SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res);

	do_log(SMB_VFS_OP_FS_CAPABILITIES, true, handle, "");

	return result;
}

static SMB_STRUCT_DIR *smb_full_audit_opendir(vfs_handle_struct *handle,
			  const char *fname, const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;

	result = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);

	do_log(SMB_VFS_OP_OPENDIR, (result != NULL), handle, "%s", fname);

	return result;
}

static SMB_STRUCT_DIR *smb_full_audit_fdopendir(vfs_handle_struct *handle,
			  files_struct *fsp, const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;

	result = SMB_VFS_NEXT_FDOPENDIR(handle, fsp, mask, attr);

	do_log(SMB_VFS_OP_FDOPENDIR, (result != NULL), handle, "%s",
			fsp_str_do_log(fsp));

	return result;
}

static SMB_STRUCT_DIRENT *smb_full_audit_readdir(vfs_handle_struct *handle,
				    SMB_STRUCT_DIR *dirp, SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_DIRENT *result;

	result = SMB_VFS_NEXT_READDIR(handle, dirp, sbuf);

	/* This operation has no reasonable error condition
	 * (End of dir is also failure), so always succeed.
	 */
	do_log(SMB_VFS_OP_READDIR, True, handle, "");

	return result;
}

static void smb_full_audit_seekdir(vfs_handle_struct *handle,
			SMB_STRUCT_DIR *dirp, long offset)
{
	SMB_VFS_NEXT_SEEKDIR(handle, dirp, offset);

	do_log(SMB_VFS_OP_SEEKDIR, True, handle, "");
	return;
}

static long smb_full_audit_telldir(vfs_handle_struct *handle,
			SMB_STRUCT_DIR *dirp)
{
	long result;

	result = SMB_VFS_NEXT_TELLDIR(handle, dirp);

	do_log(SMB_VFS_OP_TELLDIR, True, handle, "");

	return result;
}

static void smb_full_audit_rewinddir(vfs_handle_struct *handle,
			SMB_STRUCT_DIR *dirp)
{
	SMB_VFS_NEXT_REWINDDIR(handle, dirp);

	do_log(SMB_VFS_OP_REWINDDIR, True, handle, "");
	return;
}

static int smb_full_audit_mkdir(vfs_handle_struct *handle,
		       const char *path, mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_MKDIR(handle, path, mode);
	
	do_log(SMB_VFS_OP_MKDIR, (result >= 0), handle, "%s", path);

	return result;
}

static int smb_full_audit_rmdir(vfs_handle_struct *handle,
		       const char *path)
{
	int result;
	
	result = SMB_VFS_NEXT_RMDIR(handle, path);

	do_log(SMB_VFS_OP_RMDIR, (result >= 0), handle, "%s", path);

	return result;
}

static int smb_full_audit_closedir(vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp)
{
	int result;

	result = SMB_VFS_NEXT_CLOSEDIR(handle, dirp);
	
	do_log(SMB_VFS_OP_CLOSEDIR, (result >= 0), handle, "");

	return result;
}

static void smb_full_audit_init_search_op(vfs_handle_struct *handle,
			SMB_STRUCT_DIR *dirp)
{
	SMB_VFS_NEXT_INIT_SEARCH_OP(handle, dirp);

	do_log(SMB_VFS_OP_INIT_SEARCH_OP, True, handle, "");
	return;
}

static int smb_full_audit_open(vfs_handle_struct *handle,
			       struct smb_filename *smb_fname,
			       files_struct *fsp, int flags, mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);

	do_log(SMB_VFS_OP_OPEN, (result >= 0), handle, "%s|%s",
	       ((flags & O_WRONLY) || (flags & O_RDWR))?"w":"r",
	       smb_fname_str_do_log(smb_fname));

	return result;
}

static NTSTATUS smb_full_audit_create_file(vfs_handle_struct *handle,
				      struct smb_request *req,
				      uint16_t root_dir_fid,
				      struct smb_filename *smb_fname,
				      uint32_t access_mask,
				      uint32_t share_access,
				      uint32_t create_disposition,
				      uint32_t create_options,
				      uint32_t file_attributes,
				      uint32_t oplock_request,
				      uint64_t allocation_size,
				      uint32_t private_flags,
				      struct security_descriptor *sd,
				      struct ea_list *ea_list,
				      files_struct **result_fsp,
				      int *pinfo)
{
	NTSTATUS result;
	const char* str_create_disposition;

	switch (create_disposition) {
	case FILE_SUPERSEDE:
		str_create_disposition = "supersede";
		break;
	case FILE_OVERWRITE_IF:
		str_create_disposition = "overwrite_if";
		break;
	case FILE_OPEN:
		str_create_disposition = "open";
		break;
	case FILE_OVERWRITE:
		str_create_disposition = "overwrite";
		break;
	case FILE_CREATE:
		str_create_disposition = "create";
		break;
	case FILE_OPEN_IF:
		str_create_disposition = "open_if";
		break;
	default:
		str_create_disposition = "unknown";
	}

	result = SMB_VFS_NEXT_CREATE_FILE(
		handle,					/* handle */
		req,					/* req */
		root_dir_fid,				/* root_dir_fid */
		smb_fname,				/* fname */
		access_mask,				/* access_mask */
		share_access,				/* share_access */
		create_disposition,			/* create_disposition*/
		create_options,				/* create_options */
		file_attributes,			/* file_attributes */
		oplock_request,				/* oplock_request */
		allocation_size,			/* allocation_size */
		private_flags,
		sd,					/* sd */
		ea_list,				/* ea_list */
		result_fsp,				/* result */
		pinfo);					/* pinfo */

	do_log(SMB_VFS_OP_CREATE_FILE, (NT_STATUS_IS_OK(result)), handle,
	       "0x%x|%s|%s|%s", access_mask,
	       create_options & FILE_DIRECTORY_FILE ? "dir" : "file",
	       str_create_disposition, smb_fname_str_do_log(smb_fname));

	return result;
}

static int smb_full_audit_close(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;
	
	result = SMB_VFS_NEXT_CLOSE(handle, fsp);

	do_log(SMB_VFS_OP_CLOSE, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_read(vfs_handle_struct *handle, files_struct *fsp,
			  void *data, size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_READ(handle, fsp, data, n);

	do_log(SMB_VFS_OP_READ, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_pread(vfs_handle_struct *handle, files_struct *fsp,
			   void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result;

	result = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);

	do_log(SMB_VFS_OP_PREAD, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_write(vfs_handle_struct *handle, files_struct *fsp,
			   const void *data, size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_WRITE(handle, fsp, data, n);

	do_log(SMB_VFS_OP_WRITE, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_pwrite(vfs_handle_struct *handle, files_struct *fsp,
			    const void *data, size_t n,
			    SMB_OFF_T offset)
{
	ssize_t result;

	result = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);

	do_log(SMB_VFS_OP_PWRITE, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static SMB_OFF_T smb_full_audit_lseek(vfs_handle_struct *handle, files_struct *fsp,
			     SMB_OFF_T offset, int whence)
{
	ssize_t result;

	result = SMB_VFS_NEXT_LSEEK(handle, fsp, offset, whence);

	do_log(SMB_VFS_OP_LSEEK, (result != (ssize_t)-1), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_sendfile(vfs_handle_struct *handle, int tofd,
			      files_struct *fromfsp,
			      const DATA_BLOB *hdr, SMB_OFF_T offset,
			      size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_SENDFILE(handle, tofd, fromfsp, hdr, offset, n);

	do_log(SMB_VFS_OP_SENDFILE, (result >= 0), handle,
	       "%s", fsp_str_do_log(fromfsp));

	return result;
}

static ssize_t smb_full_audit_recvfile(vfs_handle_struct *handle, int fromfd,
		      files_struct *tofsp,
			      SMB_OFF_T offset,
			      size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_RECVFILE(handle, fromfd, tofsp, offset, n);

	do_log(SMB_VFS_OP_RECVFILE, (result >= 0), handle,
	       "%s", fsp_str_do_log(tofsp));

	return result;
}

static int smb_full_audit_rename(vfs_handle_struct *handle,
				 const struct smb_filename *smb_fname_src,
				 const struct smb_filename *smb_fname_dst)
{
	int result;
	
	result = SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);

	do_log(SMB_VFS_OP_RENAME, (result >= 0), handle, "%s|%s",
	       smb_fname_str_do_log(smb_fname_src),
	       smb_fname_str_do_log(smb_fname_dst));

	return result;    
}

static int smb_full_audit_fsync(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;
	
	result = SMB_VFS_NEXT_FSYNC(handle, fsp);

	do_log(SMB_VFS_OP_FSYNC, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;    
}

static int smb_full_audit_stat(vfs_handle_struct *handle,
			       struct smb_filename *smb_fname)
{
	int result;
	
	result = SMB_VFS_NEXT_STAT(handle, smb_fname);

	do_log(SMB_VFS_OP_STAT, (result >= 0), handle, "%s",
	       smb_fname_str_do_log(smb_fname));

	return result;    
}

static int smb_full_audit_fstat(vfs_handle_struct *handle, files_struct *fsp,
		       SMB_STRUCT_STAT *sbuf)
{
	int result;
	
	result = SMB_VFS_NEXT_FSTAT(handle, fsp, sbuf);

	do_log(SMB_VFS_OP_FSTAT, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_lstat(vfs_handle_struct *handle,
				struct smb_filename *smb_fname)
{
	int result;
	
	result = SMB_VFS_NEXT_LSTAT(handle, smb_fname);

	do_log(SMB_VFS_OP_LSTAT, (result >= 0), handle, "%s",
	       smb_fname_str_do_log(smb_fname));

	return result;    
}

static uint64_t smb_full_audit_get_alloc_size(vfs_handle_struct *handle,
		       files_struct *fsp, const SMB_STRUCT_STAT *sbuf)
{
	uint64_t result;

	result = SMB_VFS_NEXT_GET_ALLOC_SIZE(handle, fsp, sbuf);

	do_log(SMB_VFS_OP_GET_ALLOC_SIZE, (result != (uint64_t)-1), handle,
			"%llu", result);

	return result;
}

static int smb_full_audit_unlink(vfs_handle_struct *handle,
				 const struct smb_filename *smb_fname)
{
	int result;
	
	result = SMB_VFS_NEXT_UNLINK(handle, smb_fname);

	do_log(SMB_VFS_OP_UNLINK, (result >= 0), handle, "%s",
	       smb_fname_str_do_log(smb_fname));

	return result;
}

static int smb_full_audit_chmod(vfs_handle_struct *handle,
		       const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD(handle, path, mode);

	do_log(SMB_VFS_OP_CHMOD, (result >= 0), handle, "%s|%o", path, mode);

	return result;
}

static int smb_full_audit_fchmod(vfs_handle_struct *handle, files_struct *fsp,
			mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_FCHMOD(handle, fsp, mode);

	do_log(SMB_VFS_OP_FCHMOD, (result >= 0), handle,
	       "%s|%o", fsp_str_do_log(fsp), mode);

	return result;
}

static int smb_full_audit_chown(vfs_handle_struct *handle,
		       const char *path, uid_t uid, gid_t gid)
{
	int result;

	result = SMB_VFS_NEXT_CHOWN(handle, path, uid, gid);

	do_log(SMB_VFS_OP_CHOWN, (result >= 0), handle, "%s|%ld|%ld",
	       path, (long int)uid, (long int)gid);

	return result;
}

static int smb_full_audit_fchown(vfs_handle_struct *handle, files_struct *fsp,
			uid_t uid, gid_t gid)
{
	int result;

	result = SMB_VFS_NEXT_FCHOWN(handle, fsp, uid, gid);

	do_log(SMB_VFS_OP_FCHOWN, (result >= 0), handle, "%s|%ld|%ld",
	       fsp_str_do_log(fsp), (long int)uid, (long int)gid);

	return result;
}

static int smb_full_audit_lchown(vfs_handle_struct *handle,
		       const char *path, uid_t uid, gid_t gid)
{
	int result;

	result = SMB_VFS_NEXT_LCHOWN(handle, path, uid, gid);

	do_log(SMB_VFS_OP_LCHOWN, (result >= 0), handle, "%s|%ld|%ld",
	       path, (long int)uid, (long int)gid);

	return result;
}

static int smb_full_audit_chdir(vfs_handle_struct *handle,
		       const char *path)
{
	int result;

	result = SMB_VFS_NEXT_CHDIR(handle, path);

	do_log(SMB_VFS_OP_CHDIR, (result >= 0), handle, "chdir|%s", path);

	return result;
}

static char *smb_full_audit_getwd(vfs_handle_struct *handle,
			 char *path)
{
	char *result;

	result = SMB_VFS_NEXT_GETWD(handle, path);
	
	do_log(SMB_VFS_OP_GETWD, (result != NULL), handle, "%s", path);

	return result;
}

static int smb_full_audit_ntimes(vfs_handle_struct *handle,
				 const struct smb_filename *smb_fname,
				 struct smb_file_time *ft)
{
	int result;

	result = SMB_VFS_NEXT_NTIMES(handle, smb_fname, ft);

	do_log(SMB_VFS_OP_NTIMES, (result >= 0), handle, "%s",
	       smb_fname_str_do_log(smb_fname));

	return result;
}

static int smb_full_audit_ftruncate(vfs_handle_struct *handle, files_struct *fsp,
			   SMB_OFF_T len)
{
	int result;

	result = SMB_VFS_NEXT_FTRUNCATE(handle, fsp, len);

	do_log(SMB_VFS_OP_FTRUNCATE, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_fallocate(vfs_handle_struct *handle, files_struct *fsp,
			   enum vfs_fallocate_mode mode,
			   SMB_OFF_T offset,
			   SMB_OFF_T len)
{
	int result;

	result = SMB_VFS_NEXT_FALLOCATE(handle, fsp, mode, offset, len);

	do_log(SMB_VFS_OP_FALLOCATE, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static bool smb_full_audit_lock(vfs_handle_struct *handle, files_struct *fsp,
		       int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	bool result;

	result = SMB_VFS_NEXT_LOCK(handle, fsp, op, offset, count, type);

	do_log(SMB_VFS_OP_LOCK, result, handle, "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_kernel_flock(struct vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       uint32 share_mode, uint32 access_mask)
{
	int result;

	result = SMB_VFS_NEXT_KERNEL_FLOCK(handle, fsp, share_mode, access_mask);

	do_log(SMB_VFS_OP_KERNEL_FLOCK, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_linux_setlease(vfs_handle_struct *handle, files_struct *fsp,
                                 int leasetype)
{
        int result;

        result = SMB_VFS_NEXT_LINUX_SETLEASE(handle, fsp, leasetype);

        do_log(SMB_VFS_OP_LINUX_SETLEASE, (result >= 0), handle, "%s",
	       fsp_str_do_log(fsp));

        return result;
}

static bool smb_full_audit_getlock(vfs_handle_struct *handle, files_struct *fsp,
		       SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	bool result;

	result = SMB_VFS_NEXT_GETLOCK(handle, fsp, poffset, pcount, ptype, ppid);

	do_log(SMB_VFS_OP_GETLOCK, result, handle, "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_symlink(vfs_handle_struct *handle,
			 const char *oldpath, const char *newpath)
{
	int result;

	result = SMB_VFS_NEXT_SYMLINK(handle, oldpath, newpath);

	do_log(SMB_VFS_OP_SYMLINK, (result >= 0), handle,
	       "%s|%s", oldpath, newpath);

	return result;
}

static int smb_full_audit_readlink(vfs_handle_struct *handle,
			  const char *path, char *buf, size_t bufsiz)
{
	int result;

	result = SMB_VFS_NEXT_READLINK(handle, path, buf, bufsiz);

	do_log(SMB_VFS_OP_READLINK, (result >= 0), handle, "%s", path);

	return result;
}

static int smb_full_audit_link(vfs_handle_struct *handle,
		      const char *oldpath, const char *newpath)
{
	int result;

	result = SMB_VFS_NEXT_LINK(handle, oldpath, newpath);

	do_log(SMB_VFS_OP_LINK, (result >= 0), handle,
	       "%s|%s", oldpath, newpath);

	return result;
}

static int smb_full_audit_mknod(vfs_handle_struct *handle,
		       const char *pathname, mode_t mode, SMB_DEV_T dev)
{
	int result;

	result = SMB_VFS_NEXT_MKNOD(handle, pathname, mode, dev);

	do_log(SMB_VFS_OP_MKNOD, (result >= 0), handle, "%s", pathname);

	return result;
}

static char *smb_full_audit_realpath(vfs_handle_struct *handle,
			    const char *path)
{
	char *result;

	result = SMB_VFS_NEXT_REALPATH(handle, path);

	do_log(SMB_VFS_OP_REALPATH, (result != NULL), handle, "%s", path);

	return result;
}

static NTSTATUS smb_full_audit_notify_watch(struct vfs_handle_struct *handle,
			struct sys_notify_context *ctx,
			struct notify_entry *e,
			void (*callback)(struct sys_notify_context *ctx,
					void *private_data,
					struct notify_event *ev),
			void *private_data, void *handle_p)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_NOTIFY_WATCH(handle, ctx, e, callback, private_data, handle_p);

	do_log(SMB_VFS_OP_NOTIFY_WATCH, NT_STATUS_IS_OK(result), handle, "");

	return result;
}

static int smb_full_audit_chflags(vfs_handle_struct *handle,
			    const char *path, unsigned int flags)
{
	int result;

	result = SMB_VFS_NEXT_CHFLAGS(handle, path, flags);

	do_log(SMB_VFS_OP_CHFLAGS, (result != 0), handle, "%s", path);

	return result;
}

static struct file_id smb_full_audit_file_id_create(struct vfs_handle_struct *handle,
						    const SMB_STRUCT_STAT *sbuf)
{
	struct file_id id_zero;
	struct file_id result;

	ZERO_STRUCT(id_zero);

	result = SMB_VFS_NEXT_FILE_ID_CREATE(handle, sbuf);

	do_log(SMB_VFS_OP_FILE_ID_CREATE,
	       !file_id_equal(&id_zero, &result),
	       handle, "%s", file_id_string_tos(&result));

	return result;
}

static NTSTATUS smb_full_audit_streaminfo(vfs_handle_struct *handle,
					  struct files_struct *fsp,
					  const char *fname,
					  TALLOC_CTX *mem_ctx,
					  unsigned int *pnum_streams,
					  struct stream_struct **pstreams)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_STREAMINFO(handle, fsp, fname, mem_ctx,
					 pnum_streams, pstreams);

	do_log(SMB_VFS_OP_STREAMINFO, NT_STATUS_IS_OK(result), handle,
	       "%s", fname);

	return result;
}

static int smb_full_audit_get_real_filename(struct vfs_handle_struct *handle,
					    const char *path,
					    const char *name,
					    TALLOC_CTX *mem_ctx,
					    char **found_name)
{
	int result;

	result = SMB_VFS_NEXT_GET_REAL_FILENAME(handle, path, name, mem_ctx,
						found_name);

	do_log(SMB_VFS_OP_GET_REAL_FILENAME, (result == 0), handle,
	       "%s/%s->%s", path, name, (result == 0) ? "" : *found_name);

	return result;
}

static const char *smb_full_audit_connectpath(vfs_handle_struct *handle,
					      const char *fname)
{
	const char *result;

	result = SMB_VFS_NEXT_CONNECTPATH(handle, fname);

	do_log(SMB_VFS_OP_CONNECTPATH, result != NULL, handle,
	       "%s", fname);

	return result;
}

static NTSTATUS smb_full_audit_brl_lock_windows(struct vfs_handle_struct *handle,
					        struct byte_range_lock *br_lck,
					        struct lock_struct *plock,
					        bool blocking_lock,
						struct blocking_lock_record *blr)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_BRL_LOCK_WINDOWS(handle, br_lck, plock,
	    blocking_lock, blr);

	do_log(SMB_VFS_OP_BRL_LOCK_WINDOWS, NT_STATUS_IS_OK(result), handle,
	    "%s:%llu-%llu. type=%d. blocking=%d", fsp_str_do_log(br_lck->fsp),
	    plock->start, plock->size, plock->lock_type, blocking_lock );

	return result;
}

static bool smb_full_audit_brl_unlock_windows(struct vfs_handle_struct *handle,
				              struct messaging_context *msg_ctx,
				              struct byte_range_lock *br_lck,
				              const struct lock_struct *plock)
{
	bool result;

	result = SMB_VFS_NEXT_BRL_UNLOCK_WINDOWS(handle, msg_ctx, br_lck,
	    plock);

	do_log(SMB_VFS_OP_BRL_UNLOCK_WINDOWS, (result == 0), handle,
	    "%s:%llu-%llu:%d", fsp_str_do_log(br_lck->fsp), plock->start,
	    plock->size, plock->lock_type);

	return result;
}

static bool smb_full_audit_brl_cancel_windows(struct vfs_handle_struct *handle,
				              struct byte_range_lock *br_lck,
					      struct lock_struct *plock,
					      struct blocking_lock_record *blr)
{
	bool result;

	result = SMB_VFS_NEXT_BRL_CANCEL_WINDOWS(handle, br_lck, plock, blr);

	do_log(SMB_VFS_OP_BRL_CANCEL_WINDOWS, (result == 0), handle,
	    "%s:%llu-%llu:%d", fsp_str_do_log(br_lck->fsp), plock->start,
	    plock->size);

	return result;
}

static bool smb_full_audit_strict_lock(struct vfs_handle_struct *handle,
				       struct files_struct *fsp,
				       struct lock_struct *plock)
{
	bool result;

	result = SMB_VFS_NEXT_STRICT_LOCK(handle, fsp, plock);

	do_log(SMB_VFS_OP_STRICT_LOCK, result, handle,
	    "%s:%llu-%llu:%d", fsp_str_do_log(fsp), plock->start,
	    plock->size);

	return result;
}

static void smb_full_audit_strict_unlock(struct vfs_handle_struct *handle,
					 struct files_struct *fsp,
					 struct lock_struct *plock)
{
	SMB_VFS_NEXT_STRICT_UNLOCK(handle, fsp, plock);

	do_log(SMB_VFS_OP_STRICT_UNLOCK, true, handle,
	    "%s:%llu-%llu:%d", fsp_str_do_log(fsp), plock->start,
	    plock->size);

	return;
}

static NTSTATUS smb_full_audit_translate_name(struct vfs_handle_struct *handle,
					      const char *name,
					      enum vfs_translate_direction direction,
					      TALLOC_CTX *mem_ctx,
					      char **mapped_name)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_TRANSLATE_NAME(handle, name, direction, mem_ctx,
					     mapped_name);

	do_log(SMB_VFS_OP_TRANSLATE_NAME, NT_STATUS_IS_OK(result), handle, "");

	return result;
}

static NTSTATUS smb_full_audit_fget_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
				uint32 security_info,
				struct security_descriptor **ppdesc)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, security_info, ppdesc);

	do_log(SMB_VFS_OP_FGET_NT_ACL, NT_STATUS_IS_OK(result), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static NTSTATUS smb_full_audit_get_nt_acl(vfs_handle_struct *handle,
					  const char *name,
					  uint32 security_info,
					  struct security_descriptor **ppdesc)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_GET_NT_ACL(handle, name, security_info, ppdesc);

	do_log(SMB_VFS_OP_GET_NT_ACL, NT_STATUS_IS_OK(result), handle,
	       "%s", name);

	return result;
}

static NTSTATUS smb_full_audit_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
			      uint32 security_info_sent,
			      const struct security_descriptor *psd)
{
	NTSTATUS result;

	result = SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, security_info_sent, psd);

	do_log(SMB_VFS_OP_FSET_NT_ACL, NT_STATUS_IS_OK(result), handle, "%s",
	       fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_chmod_acl(vfs_handle_struct *handle,
			   const char *path, mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_CHMOD_ACL(handle, path, mode);

	do_log(SMB_VFS_OP_CHMOD_ACL, (result >= 0), handle,
	       "%s|%o", path, mode);

	return result;
}

static int smb_full_audit_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp,
				     mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, mode);

	do_log(SMB_VFS_OP_FCHMOD_ACL, (result >= 0), handle,
	       "%s|%o", fsp_str_do_log(fsp), mode);

	return result;
}

static int smb_full_audit_sys_acl_get_entry(vfs_handle_struct *handle,

				   SMB_ACL_T theacl, int entry_id,
				   SMB_ACL_ENTRY_T *entry_p)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_ENTRY(handle, theacl, entry_id,
						entry_p);

	do_log(SMB_VFS_OP_SYS_ACL_GET_ENTRY, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_get_tag_type(vfs_handle_struct *handle,

				      SMB_ACL_ENTRY_T entry_d,
				      SMB_ACL_TAG_T *tag_type_p)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_TAG_TYPE(handle, entry_d,
						   tag_type_p);

	do_log(SMB_VFS_OP_SYS_ACL_GET_TAG_TYPE, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_get_permset(vfs_handle_struct *handle,

				     SMB_ACL_ENTRY_T entry_d,
				     SMB_ACL_PERMSET_T *permset_p)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_PERMSET(handle, entry_d,
						  permset_p);

	do_log(SMB_VFS_OP_SYS_ACL_GET_PERMSET, (result >= 0), handle,
	       "");

	return result;
}

static void * smb_full_audit_sys_acl_get_qualifier(vfs_handle_struct *handle,

					  SMB_ACL_ENTRY_T entry_d)
{
	void *result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_QUALIFIER(handle, entry_d);

	do_log(SMB_VFS_OP_SYS_ACL_GET_QUALIFIER, (result != NULL), handle,
	       "");

	return result;
}

static SMB_ACL_T smb_full_audit_sys_acl_get_file(vfs_handle_struct *handle,
					const char *path_p,
					SMB_ACL_TYPE_T type)
{
	SMB_ACL_T result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, path_p, type);

	do_log(SMB_VFS_OP_SYS_ACL_GET_FILE, (result != NULL), handle,
	       "%s", path_p);

	return result;
}

static SMB_ACL_T smb_full_audit_sys_acl_get_fd(vfs_handle_struct *handle,
				      files_struct *fsp)
{
	SMB_ACL_T result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp);

	do_log(SMB_VFS_OP_SYS_ACL_GET_FD, (result != NULL), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_sys_acl_clear_perms(vfs_handle_struct *handle,

				     SMB_ACL_PERMSET_T permset)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_CLEAR_PERMS(handle, permset);

	do_log(SMB_VFS_OP_SYS_ACL_CLEAR_PERMS, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_add_perm(vfs_handle_struct *handle,

				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_ADD_PERM(handle, permset, perm);

	do_log(SMB_VFS_OP_SYS_ACL_ADD_PERM, (result >= 0), handle,
	       "");

	return result;
}

static char * smb_full_audit_sys_acl_to_text(vfs_handle_struct *handle,
				    SMB_ACL_T theacl,
				    ssize_t *plen)
{
	char * result;

	result = SMB_VFS_NEXT_SYS_ACL_TO_TEXT(handle, theacl, plen);

	do_log(SMB_VFS_OP_SYS_ACL_TO_TEXT, (result != NULL), handle,
	       "");

	return result;
}

static SMB_ACL_T smb_full_audit_sys_acl_init(vfs_handle_struct *handle,

				    int count)
{
	SMB_ACL_T result;

	result = SMB_VFS_NEXT_SYS_ACL_INIT(handle, count);

	do_log(SMB_VFS_OP_SYS_ACL_INIT, (result != NULL), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_create_entry(vfs_handle_struct *handle,
				      SMB_ACL_T *pacl,
				      SMB_ACL_ENTRY_T *pentry)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_CREATE_ENTRY(handle, pacl, pentry);

	do_log(SMB_VFS_OP_SYS_ACL_CREATE_ENTRY, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_set_tag_type(vfs_handle_struct *handle,

				      SMB_ACL_ENTRY_T entry,
				      SMB_ACL_TAG_T tagtype)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_SET_TAG_TYPE(handle, entry,
						   tagtype);

	do_log(SMB_VFS_OP_SYS_ACL_SET_TAG_TYPE, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_set_qualifier(vfs_handle_struct *handle,

				       SMB_ACL_ENTRY_T entry,
				       void *qual)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_SET_QUALIFIER(handle, entry, qual);

	do_log(SMB_VFS_OP_SYS_ACL_SET_QUALIFIER, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_set_permset(vfs_handle_struct *handle,

				     SMB_ACL_ENTRY_T entry,
				     SMB_ACL_PERMSET_T permset)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_SET_PERMSET(handle, entry, permset);

	do_log(SMB_VFS_OP_SYS_ACL_SET_PERMSET, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_valid(vfs_handle_struct *handle,

			       SMB_ACL_T theacl )
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_VALID(handle, theacl);

	do_log(SMB_VFS_OP_SYS_ACL_VALID, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_set_file(vfs_handle_struct *handle,

				  const char *name, SMB_ACL_TYPE_T acltype,
				  SMB_ACL_T theacl)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, name, acltype,
					       theacl);

	do_log(SMB_VFS_OP_SYS_ACL_SET_FILE, (result >= 0), handle,
	       "%s", name);

	return result;
}

static int smb_full_audit_sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
				SMB_ACL_T theacl)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, theacl);

	do_log(SMB_VFS_OP_SYS_ACL_SET_FD, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_sys_acl_delete_def_file(vfs_handle_struct *handle,

					 const char *path)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, path);

	do_log(SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE, (result >= 0), handle,
	       "%s", path);

	return result;
}

static int smb_full_audit_sys_acl_get_perm(vfs_handle_struct *handle,

				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_GET_PERM(handle, permset, perm);

	do_log(SMB_VFS_OP_SYS_ACL_GET_PERM, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_free_text(vfs_handle_struct *handle,

				   char *text)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_FREE_TEXT(handle, text);

	do_log(SMB_VFS_OP_SYS_ACL_FREE_TEXT, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_free_acl(vfs_handle_struct *handle,

				  SMB_ACL_T posix_acl)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_FREE_ACL(handle, posix_acl);

	do_log(SMB_VFS_OP_SYS_ACL_FREE_ACL, (result >= 0), handle,
	       "");

	return result;
}

static int smb_full_audit_sys_acl_free_qualifier(vfs_handle_struct *handle,
					void *qualifier,
					SMB_ACL_TAG_T tagtype)
{
	int result;

	result = SMB_VFS_NEXT_SYS_ACL_FREE_QUALIFIER(handle, qualifier,
						     tagtype);

	do_log(SMB_VFS_OP_SYS_ACL_FREE_QUALIFIER, (result >= 0), handle,
	       "");

	return result;
}

static ssize_t smb_full_audit_getxattr(struct vfs_handle_struct *handle,
			      const char *path,
			      const char *name, void *value, size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_GETXATTR(handle, path, name, value, size);

	do_log(SMB_VFS_OP_GETXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static ssize_t smb_full_audit_lgetxattr(struct vfs_handle_struct *handle,
			       const char *path, const char *name,
			       void *value, size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_LGETXATTR(handle, path, name, value, size);

	do_log(SMB_VFS_OP_LGETXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static ssize_t smb_full_audit_fgetxattr(struct vfs_handle_struct *handle,
			       struct files_struct *fsp,
			       const char *name, void *value, size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_FGETXATTR(handle, fsp, name, value, size);

	do_log(SMB_VFS_OP_FGETXATTR, (result >= 0), handle,
	       "%s|%s", fsp_str_do_log(fsp), name);

	return result;
}

static ssize_t smb_full_audit_listxattr(struct vfs_handle_struct *handle,
			       const char *path, char *list, size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_LISTXATTR(handle, path, list, size);

	do_log(SMB_VFS_OP_LISTXATTR, (result >= 0), handle, "%s", path);

	return result;
}

static ssize_t smb_full_audit_llistxattr(struct vfs_handle_struct *handle,
				const char *path, char *list, size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_LLISTXATTR(handle, path, list, size);

	do_log(SMB_VFS_OP_LLISTXATTR, (result >= 0), handle, "%s", path);

	return result;
}

static ssize_t smb_full_audit_flistxattr(struct vfs_handle_struct *handle,
				struct files_struct *fsp, char *list,
				size_t size)
{
	ssize_t result;

	result = SMB_VFS_NEXT_FLISTXATTR(handle, fsp, list, size);

	do_log(SMB_VFS_OP_FLISTXATTR, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_removexattr(struct vfs_handle_struct *handle,
			     const char *path,
			     const char *name)
{
	int result;

	result = SMB_VFS_NEXT_REMOVEXATTR(handle, path, name);

	do_log(SMB_VFS_OP_REMOVEXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static int smb_full_audit_lremovexattr(struct vfs_handle_struct *handle,
			      const char *path,
			      const char *name)
{
	int result;

	result = SMB_VFS_NEXT_LREMOVEXATTR(handle, path, name);

	do_log(SMB_VFS_OP_LREMOVEXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static int smb_full_audit_fremovexattr(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      const char *name)
{
	int result;

	result = SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, name);

	do_log(SMB_VFS_OP_FREMOVEXATTR, (result >= 0), handle,
	       "%s|%s", fsp_str_do_log(fsp), name);

	return result;
}

static int smb_full_audit_setxattr(struct vfs_handle_struct *handle,
			  const char *path,
			  const char *name, const void *value, size_t size,
			  int flags)
{
	int result;

	result = SMB_VFS_NEXT_SETXATTR(handle, path, name, value, size,
				       flags);

	do_log(SMB_VFS_OP_SETXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static int smb_full_audit_lsetxattr(struct vfs_handle_struct *handle,
			   const char *path,
			   const char *name, const void *value, size_t size,
			   int flags)
{
	int result;

	result = SMB_VFS_NEXT_LSETXATTR(handle, path, name, value, size,
					flags);

	do_log(SMB_VFS_OP_LSETXATTR, (result >= 0), handle,
	       "%s|%s", path, name);

	return result;
}

static int smb_full_audit_fsetxattr(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, const char *name,
			   const void *value, size_t size, int flags)
{
	int result;

	result = SMB_VFS_NEXT_FSETXATTR(handle, fsp, name, value, size, flags);

	do_log(SMB_VFS_OP_FSETXATTR, (result >= 0), handle,
	       "%s|%s", fsp_str_do_log(fsp), name);

	return result;
}

static int smb_full_audit_aio_read(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_READ(handle, fsp, aiocb);
	do_log(SMB_VFS_OP_AIO_READ, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_aio_write(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_WRITE(handle, fsp, aiocb);
	do_log(SMB_VFS_OP_AIO_WRITE, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static ssize_t smb_full_audit_aio_return(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);
	do_log(SMB_VFS_OP_AIO_RETURN, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_aio_cancel(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_CANCEL(handle, fsp, aiocb);
	do_log(SMB_VFS_OP_AIO_CANCEL, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_aio_error(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_ERROR(handle, fsp, aiocb);
	do_log(SMB_VFS_OP_AIO_ERROR, (result >= 0), handle,
	       "%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_aio_fsync(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_STRUCT_AIOCB *aiocb)
{
	int result;

	result = SMB_VFS_NEXT_AIO_FSYNC(handle, fsp, op, aiocb);
	do_log(SMB_VFS_OP_AIO_FSYNC, (result >= 0), handle,
		"%s", fsp_str_do_log(fsp));

	return result;
}

static int smb_full_audit_aio_suspend(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_AIOCB * const aiocb[], int n, const struct timespec *ts)
{
	int result;

	result = SMB_VFS_NEXT_AIO_SUSPEND(handle, fsp, aiocb, n, ts);
	do_log(SMB_VFS_OP_AIO_SUSPEND, (result >= 0), handle,
		"%s", fsp_str_do_log(fsp));

	return result;
}

static bool smb_full_audit_aio_force(struct vfs_handle_struct *handle,
				     struct files_struct *fsp)
{
	bool result;

	result = SMB_VFS_NEXT_AIO_FORCE(handle, fsp);
	do_log(SMB_VFS_OP_AIO_FORCE, result, handle,
		"%s", fsp_str_do_log(fsp));

	return result;
}

static bool smb_full_audit_is_offline(struct vfs_handle_struct *handle,
				      const struct smb_filename *fname,
				      SMB_STRUCT_STAT *sbuf)
{
	bool result;

	result = SMB_VFS_NEXT_IS_OFFLINE(handle, fname, sbuf);
	do_log(SMB_VFS_OP_IS_OFFLINE, result, handle, "%s",
	       smb_fname_str_do_log(fname));
	return result;
}

static int smb_full_audit_set_offline(struct vfs_handle_struct *handle,
				      const struct smb_filename *fname)
{
	int result;

	result = SMB_VFS_NEXT_SET_OFFLINE(handle, fname);
	do_log(SMB_VFS_OP_SET_OFFLINE, result >= 0, handle, "%s",
	       smb_fname_str_do_log(fname));
	return result;
}

static struct vfs_fn_pointers vfs_full_audit_fns = {

	/* Disk operations */

	.connect_fn = smb_full_audit_connect,
	.disconnect = smb_full_audit_disconnect,
	.disk_free = smb_full_audit_disk_free,
	.get_quota = smb_full_audit_get_quota,
	.set_quota = smb_full_audit_set_quota,
	.get_shadow_copy_data = smb_full_audit_get_shadow_copy_data,
	.statvfs = smb_full_audit_statvfs,
	.fs_capabilities = smb_full_audit_fs_capabilities,
	.opendir = smb_full_audit_opendir,
	.fdopendir = smb_full_audit_fdopendir,
	.readdir = smb_full_audit_readdir,
	.seekdir = smb_full_audit_seekdir,
	.telldir = smb_full_audit_telldir,
	.rewind_dir = smb_full_audit_rewinddir,
	.mkdir = smb_full_audit_mkdir,
	.rmdir = smb_full_audit_rmdir,
	.closedir = smb_full_audit_closedir,
	.init_search_op = smb_full_audit_init_search_op,
	.open_fn = smb_full_audit_open,
	.create_file = smb_full_audit_create_file,
	.close_fn = smb_full_audit_close,
	.vfs_read = smb_full_audit_read,
	.pread = smb_full_audit_pread,
	.write = smb_full_audit_write,
	.pwrite = smb_full_audit_pwrite,
	.lseek = smb_full_audit_lseek,
	.sendfile = smb_full_audit_sendfile,
	.recvfile = smb_full_audit_recvfile,
	.rename = smb_full_audit_rename,
	.fsync = smb_full_audit_fsync,
	.stat = smb_full_audit_stat,
	.fstat = smb_full_audit_fstat,
	.lstat = smb_full_audit_lstat,
	.get_alloc_size = smb_full_audit_get_alloc_size,
	.unlink = smb_full_audit_unlink,
	.chmod = smb_full_audit_chmod,
	.fchmod = smb_full_audit_fchmod,
	.chown = smb_full_audit_chown,
	.fchown = smb_full_audit_fchown,
	.lchown = smb_full_audit_lchown,
	.chdir = smb_full_audit_chdir,
	.getwd = smb_full_audit_getwd,
	.ntimes = smb_full_audit_ntimes,
	.ftruncate = smb_full_audit_ftruncate,
	.fallocate = smb_full_audit_fallocate,
	.lock = smb_full_audit_lock,
	.kernel_flock = smb_full_audit_kernel_flock,
	.linux_setlease = smb_full_audit_linux_setlease,
	.getlock = smb_full_audit_getlock,
	.symlink = smb_full_audit_symlink,
	.vfs_readlink = smb_full_audit_readlink,
	.link = smb_full_audit_link,
	.mknod = smb_full_audit_mknod,
	.realpath = smb_full_audit_realpath,
	.notify_watch = smb_full_audit_notify_watch,
	.chflags = smb_full_audit_chflags,
	.file_id_create = smb_full_audit_file_id_create,
	.streaminfo = smb_full_audit_streaminfo,
	.get_real_filename = smb_full_audit_get_real_filename,
	.connectpath = smb_full_audit_connectpath,
	.brl_lock_windows = smb_full_audit_brl_lock_windows,
	.brl_unlock_windows = smb_full_audit_brl_unlock_windows,
	.brl_cancel_windows = smb_full_audit_brl_cancel_windows,
	.strict_lock = smb_full_audit_strict_lock,
	.strict_unlock = smb_full_audit_strict_unlock,
	.translate_name = smb_full_audit_translate_name,
	.fget_nt_acl = smb_full_audit_fget_nt_acl,
	.get_nt_acl = smb_full_audit_get_nt_acl,
	.fset_nt_acl = smb_full_audit_fset_nt_acl,
	.chmod_acl = smb_full_audit_chmod_acl,
	.fchmod_acl = smb_full_audit_fchmod_acl,
	.sys_acl_get_entry = smb_full_audit_sys_acl_get_entry,
	.sys_acl_get_tag_type = smb_full_audit_sys_acl_get_tag_type,
	.sys_acl_get_permset = smb_full_audit_sys_acl_get_permset,
	.sys_acl_get_qualifier = smb_full_audit_sys_acl_get_qualifier,
	.sys_acl_get_file = smb_full_audit_sys_acl_get_file,
	.sys_acl_get_fd = smb_full_audit_sys_acl_get_fd,
	.sys_acl_clear_perms = smb_full_audit_sys_acl_clear_perms,
	.sys_acl_add_perm = smb_full_audit_sys_acl_add_perm,
	.sys_acl_to_text = smb_full_audit_sys_acl_to_text,
	.sys_acl_init = smb_full_audit_sys_acl_init,
	.sys_acl_create_entry = smb_full_audit_sys_acl_create_entry,
	.sys_acl_set_tag_type = smb_full_audit_sys_acl_set_tag_type,
	.sys_acl_set_qualifier = smb_full_audit_sys_acl_set_qualifier,
	.sys_acl_set_permset = smb_full_audit_sys_acl_set_permset,
	.sys_acl_valid = smb_full_audit_sys_acl_valid,
	.sys_acl_set_file = smb_full_audit_sys_acl_set_file,
	.sys_acl_set_fd = smb_full_audit_sys_acl_set_fd,
	.sys_acl_delete_def_file = smb_full_audit_sys_acl_delete_def_file,
	.sys_acl_get_perm = smb_full_audit_sys_acl_get_perm,
	.sys_acl_free_text = smb_full_audit_sys_acl_free_text,
	.sys_acl_free_acl = smb_full_audit_sys_acl_free_acl,
	.sys_acl_free_qualifier = smb_full_audit_sys_acl_free_qualifier,
	.getxattr = smb_full_audit_getxattr,
	.lgetxattr = smb_full_audit_lgetxattr,
	.fgetxattr = smb_full_audit_fgetxattr,
	.listxattr = smb_full_audit_listxattr,
	.llistxattr = smb_full_audit_llistxattr,
	.flistxattr = smb_full_audit_flistxattr,
	.removexattr = smb_full_audit_removexattr,
	.lremovexattr = smb_full_audit_lremovexattr,
	.fremovexattr = smb_full_audit_fremovexattr,
	.setxattr = smb_full_audit_setxattr,
	.lsetxattr = smb_full_audit_lsetxattr,
	.fsetxattr = smb_full_audit_fsetxattr,
	.aio_read = smb_full_audit_aio_read,
	.aio_write = smb_full_audit_aio_write,
	.aio_return_fn = smb_full_audit_aio_return,
	.aio_cancel = smb_full_audit_aio_cancel,
	.aio_error_fn = smb_full_audit_aio_error,
	.aio_fsync = smb_full_audit_aio_fsync,
	.aio_suspend = smb_full_audit_aio_suspend,
	.aio_force = smb_full_audit_aio_force,
	.is_offline = smb_full_audit_is_offline,
	.set_offline = smb_full_audit_set_offline,
};

NTSTATUS vfs_full_audit_init(void)
{
	NTSTATUS ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
					"full_audit", &vfs_full_audit_fns);
	
	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_full_audit_debug_level = debug_add_class("full_audit");
	if (vfs_full_audit_debug_level == -1) {
		vfs_full_audit_debug_level = DBGC_VFS;
		DEBUG(0, ("vfs_full_audit: Couldn't register custom debugging "
			  "class!\n"));
	} else {
		DEBUG(10, ("vfs_full_audit: Debug class number of "
			   "'full_audit': %d\n", vfs_full_audit_debug_level));
	}
	
	return ret;
}
