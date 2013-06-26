/* 
 * Auditing VFS module for samba.  Log selected file operations to syslog
 * facility.
 *
 * Copyright (C) Tim Potter			1999-2000
 * Copyright (C) Alexander Bokovoy		2002
 * Copyright (C) Stefan (metze) Metzmacher	2002
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


#include "includes.h"
#include "system/filesys.h"
#include "system/syslog.h"
#include "smbd/smbd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

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

	facility = lp_parm_enum(SNUM(handle->conn), "audit", "facility", enum_log_facilities, LOG_USER);

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

	priority = lp_parm_enum(SNUM(handle->conn), "audit", "priority",
				enum_log_priorities, LOG_NOTICE);
	if (priority == -1) {
		priority = LOG_WARNING;
	}

	return priority;
}

/* Implementation of vfs_ops.  Pass everything on to the default
   operation but log event first. */

static int audit_connect(vfs_handle_struct *handle, const char *svc, const char *user)
{
	int result;

	result = SMB_VFS_NEXT_CONNECT(handle, svc, user);
	if (result < 0) {
		return result;
	}

	openlog("smbd_audit", LOG_PID, audit_syslog_facility(handle));

	syslog(audit_syslog_priority(handle), "connect to service %s by user %s\n", 
	       svc, user);

	return 0;
}

static void audit_disconnect(vfs_handle_struct *handle)
{
	syslog(audit_syslog_priority(handle), "disconnected\n");
	SMB_VFS_NEXT_DISCONNECT(handle);

	return;
}

static SMB_STRUCT_DIR *audit_opendir(vfs_handle_struct *handle, const char *fname, const char *mask, uint32 attr)
{
	SMB_STRUCT_DIR *result;
	
	result = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);

	syslog(audit_syslog_priority(handle), "opendir %s %s%s\n",
	       fname,
	       (result == NULL) ? "failed: " : "",
	       (result == NULL) ? strerror(errno) : "");

	return result;
}

static int audit_mkdir(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int result;
	
	result = SMB_VFS_NEXT_MKDIR(handle, path, mode);
	
	syslog(audit_syslog_priority(handle), "mkdir %s %s%s\n", 
	       path,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_rmdir(vfs_handle_struct *handle, const char *path)
{
	int result;

	result = SMB_VFS_NEXT_RMDIR(handle, path);

	syslog(audit_syslog_priority(handle), "rmdir %s %s%s\n", 
	       path, 
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_open(vfs_handle_struct *handle,
		      struct smb_filename *smb_fname, files_struct *fsp,
		      int flags, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);

	syslog(audit_syslog_priority(handle), "open %s (fd %d) %s%s%s\n", 
	       smb_fname->base_name, result,
	       ((flags & O_WRONLY) || (flags & O_RDWR)) ? "for writing " : "", 
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_close(vfs_handle_struct *handle, files_struct *fsp)
{
	int result;

	result = SMB_VFS_NEXT_CLOSE(handle, fsp);

	syslog(audit_syslog_priority(handle), "close fd %d %s%s\n",
	       fsp->fh->fd,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_rename(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname_src,
			const struct smb_filename *smb_fname_dst)
{
	int result;

	result = SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);

	syslog(audit_syslog_priority(handle), "rename %s -> %s %s%s\n",
	       smb_fname_src->base_name,
	       smb_fname_dst->base_name,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;    
}

static int audit_unlink(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname)
{
	int result;

	result = SMB_VFS_NEXT_UNLINK(handle, smb_fname);

	syslog(audit_syslog_priority(handle), "unlink %s %s%s\n",
	       smb_fname->base_name,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_chmod(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD(handle, path, mode);

	syslog(audit_syslog_priority(handle), "chmod %s mode 0x%x %s%s\n",
	       path, mode,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_chmod_acl(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD_ACL(handle, path, mode);

	syslog(audit_syslog_priority(handle), "chmod_acl %s mode 0x%x %s%s\n",
	       path, mode,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_fchmod(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD(handle, fsp, mode);

	syslog(audit_syslog_priority(handle), "fchmod %s mode 0x%x %s%s\n",
	       fsp->fsp_name->base_name, mode,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static int audit_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, mode);

	syslog(audit_syslog_priority(handle), "fchmod_acl %s mode 0x%x %s%s\n",
	       fsp->fsp_name->base_name, mode,
	       (result < 0) ? "failed: " : "",
	       (result < 0) ? strerror(errno) : "");

	return result;
}

static struct vfs_fn_pointers vfs_audit_fns = {
	.connect_fn = audit_connect,
	.disconnect = audit_disconnect,
	.opendir = audit_opendir,
	.mkdir = audit_mkdir,
	.rmdir = audit_rmdir,
	.open_fn = audit_open,
	.close_fn = audit_close,
	.rename = audit_rename,
	.unlink = audit_unlink,
	.chmod = audit_chmod,
	.fchmod = audit_fchmod,
	.chmod_acl = audit_chmod_acl,
	.fchmod_acl = audit_fchmod_acl
};

NTSTATUS vfs_audit_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "audit",
				&vfs_audit_fns);
}
