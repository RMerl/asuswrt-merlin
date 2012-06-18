/* 
   Unix SMB/CIFS implementation.
   VFS wrapper macros
   Copyright (C) Stefan (metze) Metzmacher	2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _VFS_MACROS_H
#define _VFS_MACROS_H

/*******************************************************************
 Don't access conn->vfs.ops.* directly!!!
 Use this macros!
 (Fixes should go also into the vfs_opaque_* and vfs_next_* macros!)
********************************************************************/

/* Disk operations */    
#define SMB_VFS_CONNECT(conn, service, user) ((conn)->vfs.ops.connect_fn((conn)->vfs.handles.connect_hnd, (service), (user)))
#define SMB_VFS_DISCONNECT(conn) ((conn)->vfs.ops.disconnect((conn)->vfs.handles.disconnect))
#define SMB_VFS_DISK_FREE(conn, path, small_query, bsize, dfree ,dsize) ((conn)->vfs.ops.disk_free((conn)->vfs.handles.disk_free, (path), (small_query), (bsize), (dfree), (dsize)))
#define SMB_VFS_GET_QUOTA(conn, qtype, id, qt) ((conn)->vfs.ops.get_quota((conn)->vfs.handles.get_quota, (qtype), (id), (qt)))
#define SMB_VFS_SET_QUOTA(conn, qtype, id, qt) ((conn)->vfs.ops.set_quota((conn)->vfs.handles.set_quota, (qtype), (id), (qt)))
#define SMB_VFS_GET_SHADOW_COPY_DATA(fsp,shadow_copy_data,labels) ((fsp)->conn->vfs.ops.get_shadow_copy_data((fsp)->conn->vfs.handles.get_shadow_copy_data,(fsp),(shadow_copy_data),(labels)))
#define SMB_VFS_STATVFS(conn, path, statbuf) ((conn)->vfs.ops.statvfs((conn)->vfs.handles.statvfs, (path), (statbuf)))

/* Directory operations */
#define SMB_VFS_OPENDIR(conn, fname, mask, attr) ((conn)->vfs.ops.opendir((conn)->vfs.handles.opendir, (fname), (mask), (attr)))
#define SMB_VFS_READDIR(conn, dirp) ((conn)->vfs.ops.readdir((conn)->vfs.handles.readdir, (dirp)))
#define SMB_VFS_SEEKDIR(conn, dirp, offset) ((conn)->vfs.ops.seekdir((conn)->vfs.handles.seekdir, (dirp), (offset)))
#define SMB_VFS_TELLDIR(conn, dirp) ((conn)->vfs.ops.telldir((conn)->vfs.handles.telldir, (dirp)))
#define SMB_VFS_REWINDDIR(conn, dirp) ((conn)->vfs.ops.rewind_dir((conn)->vfs.handles.rewind_dir, (dirp)))
#define SMB_VFS_MKDIR(conn, path, mode) ((conn)->vfs.ops.mkdir((conn)->vfs.handles.mkdir,(path), (mode)))
#define SMB_VFS_RMDIR(conn, path) ((conn)->vfs.ops.rmdir((conn)->vfs.handles.rmdir, (path)))
#define SMB_VFS_CLOSEDIR(conn, dir) ((conn)->vfs.ops.closedir((conn)->vfs.handles.closedir, dir))
    
/* File operations */
#define SMB_VFS_OPEN(conn, fname, fsp, flags, mode) (((conn)->vfs.ops.open)((conn)->vfs.handles.open, (fname), (fsp), (flags), (mode)))
#define SMB_VFS_CLOSE(fsp, fd) ((fsp)->conn->vfs.ops.close_fn((fsp)->conn->vfs.handles.close_hnd, (fsp), (fd)))
#define SMB_VFS_READ(fsp, fd, data, n) ((fsp)->conn->vfs.ops.read((fsp)->conn->vfs.handles.read, (fsp), (fd), (data), (n)))
#define SMB_VFS_PREAD(fsp, fd, data, n, off) ((fsp)->conn->vfs.ops.pread((fsp)->conn->vfs.handles.pread, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_WRITE(fsp, fd, data, n) ((fsp)->conn->vfs.ops.write((fsp)->conn->vfs.handles.write, (fsp), (fd), (data), (n)))
#define SMB_VFS_PWRITE(fsp, fd, data, n, off) ((fsp)->conn->vfs.ops.pwrite((fsp)->conn->vfs.handles.pwrite, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_LSEEK(fsp, fd, offset, whence) ((fsp)->conn->vfs.ops.lseek((fsp)->conn->vfs.handles.lseek, (fsp), (fd), (offset), (whence)))
#define SMB_VFS_SENDFILE(tofd, fsp, fromfd, header, offset, count) ((fsp)->conn->vfs.ops.sendfile((fsp)->conn->vfs.handles.sendfile, (tofd), (fsp), (fromfd), (header), (offset), (count)))
#define SMB_VFS_RENAME(conn, old, new) ((conn)->vfs.ops.rename((conn)->vfs.handles.rename, (old), (new)))
#define SMB_VFS_FSYNC(fsp, fd) ((fsp)->conn->vfs.ops.fsync((fsp)->conn->vfs.handles.fsync, (fsp), (fd)))
#define SMB_VFS_STAT(conn, fname, sbuf) ((conn)->vfs.ops.stat((conn)->vfs.handles.stat, (fname), (sbuf)))
#define SMB_VFS_FSTAT(fsp, fd, sbuf) ((fsp)->conn->vfs.ops.fstat((fsp)->conn->vfs.handles.fstat, (fsp) ,(fd) ,(sbuf)))
#define SMB_VFS_LSTAT(conn, path, sbuf) ((conn)->vfs.ops.lstat((conn)->vfs.handles.lstat, (path), (sbuf)))
#define SMB_VFS_UNLINK(conn, path) ((conn)->vfs.ops.unlink((conn)->vfs.handles.unlink, (path)))
#define SMB_VFS_CHMOD(conn, path, mode) ((conn)->vfs.ops.chmod((conn)->vfs.handles.chmod, (path), (mode)))
#define SMB_VFS_FCHMOD(fsp, fd, mode) ((fsp)->conn->vfs.ops.fchmod((fsp)->conn->vfs.handles.fchmod, (fsp), (fd), (mode)))
#define SMB_VFS_CHOWN(conn, path, uid, gid) ((conn)->vfs.ops.chown((conn)->vfs.handles.chown, (path), (uid), (gid)))
#define SMB_VFS_FCHOWN(fsp, fd, uid, gid) ((fsp)->conn->vfs.ops.fchown((fsp)->conn->vfs.handles.fchown, (fsp), (fd), (uid), (gid)))
#define SMB_VFS_CHDIR(conn, path) ((conn)->vfs.ops.chdir((conn)->vfs.handles.chdir, (path)))
#define SMB_VFS_GETWD(conn, buf) ((conn)->vfs.ops.getwd((conn)->vfs.handles.getwd, (buf)))
#define SMB_VFS_NTIMES(conn, path, ts) ((conn)->vfs.ops.ntimes((conn)->vfs.handles.ntimes, (path), (ts)))
#define SMB_VFS_FTRUNCATE(fsp, fd, offset) ((fsp)->conn->vfs.ops.ftruncate((fsp)->conn->vfs.handles.ftruncate, (fsp), (fd), (offset)))
#define SMB_VFS_LOCK(fsp, fd, op, offset, count, type) ((fsp)->conn->vfs.ops.lock((fsp)->conn->vfs.handles.lock, (fsp), (fd) ,(op), (offset), (count), (type)))
#define SMB_VFS_KERNEL_FLOCK(fsp, fd, share_mode) ((fsp)->conn->vfs.ops.kernel_flock((fsp)->conn->vfs.handles.kernel_flock, (fsp), (fd), (share_mode)))
#define SMB_VFS_LINUX_SETLEASE(fsp, fd, leasetype) ((fsp)->conn->vfs.ops.linux_setlease((fsp)->conn->vfs.handles.linux_setlease, (fsp), (fd), (leasetype)))
#define SMB_VFS_GETLOCK(fsp, fd, poffset, pcount, ptype, ppid) ((fsp)->conn->vfs.ops.getlock((fsp)->conn->vfs.handles.getlock, (fsp), (fd) ,(poffset), (pcount), (ptype), (ppid)))
#define SMB_VFS_SYMLINK(conn, oldpath, newpath) ((conn)->vfs.ops.symlink((conn)->vfs.handles.symlink, (oldpath), (newpath)))
#define SMB_VFS_READLINK(conn, path, buf, bufsiz) ((conn)->vfs.ops.readlink((conn)->vfs.handles.readlink, (path), (buf), (bufsiz)))
#define SMB_VFS_LINK(conn, oldpath, newpath) ((conn)->vfs.ops.link((conn)->vfs.handles.link, (oldpath), (newpath)))
#define SMB_VFS_MKNOD(conn, path, mode, dev) ((conn)->vfs.ops.mknod((conn)->vfs.handles.mknod, (path), (mode), (dev)))
#define SMB_VFS_REALPATH(conn, path, resolved_path) ((conn)->vfs.ops.realpath((conn)->vfs.handles.realpath, (path), (resolved_path)))
#define SMB_VFS_NOTIFY_WATCH(conn, ctx, e, callback, private_data, handle_p) ((conn)->vfs.ops.notify_watch((conn)->vfs.handles.notify_watch, (ctx), (e), (callback), (private_data), (handle_p)))
#define SMB_VFS_CHFLAGS(conn, path, flags) ((conn)->vfs.ops.chflags((conn)->vfs.handles.chflags, (path), (flags)))

/* NT ACL operations. */
#define SMB_VFS_FGET_NT_ACL(fsp, fd, security_info, ppdesc) ((fsp)->conn->vfs.ops.fget_nt_acl((fsp)->conn->vfs.handles.fget_nt_acl, (fsp), (fd), (security_info), (ppdesc)))
#define SMB_VFS_GET_NT_ACL(fsp, name, security_info, ppdesc) ((fsp)->conn->vfs.ops.get_nt_acl((fsp)->conn->vfs.handles.get_nt_acl, (fsp), (name), (security_info), (ppdesc)))
#define SMB_VFS_FSET_NT_ACL(fsp, fd, security_info_sent, psd) ((fsp)->conn->vfs.ops.fset_nt_acl((fsp)->conn->vfs.handles.fset_nt_acl, (fsp), (fd), (security_info_sent), (psd)))
#define SMB_VFS_SET_NT_ACL(fsp, name, security_info_sent, psd) ((fsp)->conn->vfs.ops.set_nt_acl((fsp)->conn->vfs.handles.set_nt_acl, (fsp), (name), (security_info_sent), (psd)))

/* POSIX ACL operations. */
#define SMB_VFS_CHMOD_ACL(conn, name, mode) ((conn)->vfs.ops.chmod_acl((conn)->vfs.handles.chmod_acl, (name), (mode)))
#define SMB_VFS_FCHMOD_ACL(fsp, fd, mode) ((fsp)->conn->vfs.ops.fchmod_acl((fsp)->conn->vfs.handles.chmod_acl, (fsp), (fd), (mode)))

#define SMB_VFS_SYS_ACL_GET_ENTRY(conn, theacl, entry_id, entry_p) ((conn)->vfs.ops.sys_acl_get_entry((conn)->vfs.handles.sys_acl_get_entry, (theacl), (entry_id), (entry_p)))
#define SMB_VFS_SYS_ACL_GET_TAG_TYPE(conn, entry_d, tag_type_p) ((conn)->vfs.ops.sys_acl_get_tag_type((conn)->vfs.handles.sys_acl_get_tag_type, (entry_d), (tag_type_p)))
#define SMB_VFS_SYS_ACL_GET_PERMSET(conn, entry_d, permset_p) ((conn)->vfs.ops.sys_acl_get_permset((conn)->vfs.handles.sys_acl_get_permset, (entry_d), (permset_p)))
#define SMB_VFS_SYS_ACL_GET_QUALIFIER(conn, entry_d) ((conn)->vfs.ops.sys_acl_get_qualifier((conn)->vfs.handles.sys_acl_get_qualifier, (entry_d)))
#define SMB_VFS_SYS_ACL_GET_FILE(conn, path_p, type) ((conn)->vfs.ops.sys_acl_get_file((conn)->vfs.handles.sys_acl_get_file, (path_p), (type)))
#define SMB_VFS_SYS_ACL_GET_FD(fsp, fd) ((fsp)->conn->vfs.ops.sys_acl_get_fd((fsp)->conn->vfs.handles.sys_acl_get_fd, (fsp), (fd)))
#define SMB_VFS_SYS_ACL_CLEAR_PERMS(conn, permset) ((conn)->vfs.ops.sys_acl_clear_perms((conn)->vfs.handles.sys_acl_clear_perms, (permset)))
#define SMB_VFS_SYS_ACL_ADD_PERM(conn, permset, perm) ((conn)->vfs.ops.sys_acl_add_perm((conn)->vfs.handles.sys_acl_add_perm, (permset), (perm)))
#define SMB_VFS_SYS_ACL_TO_TEXT(conn, theacl, plen) ((conn)->vfs.ops.sys_acl_to_text((conn)->vfs.handles.sys_acl_to_text, (theacl), (plen)))
#define SMB_VFS_SYS_ACL_INIT(conn, count) ((conn)->vfs.ops.sys_acl_init((conn)->vfs.handles.sys_acl_init, (count)))
#define SMB_VFS_SYS_ACL_CREATE_ENTRY(conn, pacl, pentry) ((conn)->vfs.ops.sys_acl_create_entry((conn)->vfs.handles.sys_acl_create_entry, (pacl), (pentry)))
#define SMB_VFS_SYS_ACL_SET_TAG_TYPE(conn, entry, tagtype) ((conn)->vfs.ops.sys_acl_set_tag_type((conn)->vfs.handles.sys_acl_set_tag_type, (entry), (tagtype)))
#define SMB_VFS_SYS_ACL_SET_QUALIFIER(conn, entry, qual) ((conn)->vfs.ops.sys_acl_set_qualifier((conn)->vfs.handles.sys_acl_set_qualifier, (entry), (qual)))
#define SMB_VFS_SYS_ACL_SET_PERMSET(conn, entry, permset) ((conn)->vfs.ops.sys_acl_set_permset((conn)->vfs.handles.sys_acl_set_permset, (entry), (permset)))
#define SMB_VFS_SYS_ACL_VALID(conn, theacl) ((conn)->vfs.ops.sys_acl_valid((conn)->vfs.handles.sys_acl_valid, (theacl)))
#define SMB_VFS_SYS_ACL_SET_FILE(conn, name, acltype, theacl) ((conn)->vfs.ops.sys_acl_set_file((conn)->vfs.handles.sys_acl_set_file, (name), (acltype), (theacl)))
#define SMB_VFS_SYS_ACL_SET_FD(fsp, fd, theacl) ((fsp)->conn->vfs.ops.sys_acl_set_fd((fsp)->conn->vfs.handles.sys_acl_set_fd, (fsp), (fd), (theacl)))
#define SMB_VFS_SYS_ACL_DELETE_DEF_FILE(conn, path) ((conn)->vfs.ops.sys_acl_delete_def_file((conn)->vfs.handles.sys_acl_delete_def_file, (path)))
#define SMB_VFS_SYS_ACL_GET_PERM(conn, permset, perm) ((conn)->vfs.ops.sys_acl_get_perm((conn)->vfs.handles.sys_acl_get_perm, (permset), (perm)))
#define SMB_VFS_SYS_ACL_FREE_TEXT(conn, text) ((conn)->vfs.ops.sys_acl_free_text((conn)->vfs.handles.sys_acl_free_text, (text)))
#define SMB_VFS_SYS_ACL_FREE_ACL(conn, posix_acl) ((conn)->vfs.ops.sys_acl_free_acl((conn)->vfs.handles.sys_acl_free_acl, (posix_acl)))
#define SMB_VFS_SYS_ACL_FREE_QUALIFIER(conn, qualifier, tagtype) ((conn)->vfs.ops.sys_acl_free_qualifier((conn)->vfs.handles.sys_acl_free_qualifier, (qualifier), (tagtype)))

/* EA operations. */
#define SMB_VFS_GETXATTR(conn,path,name,value,size) ((conn)->vfs.ops.getxattr((conn)->vfs.handles.getxattr,(path),(name),(value),(size)))
#define SMB_VFS_LGETXATTR(conn,path,name,value,size) ((conn)->vfs.ops.lgetxattr((conn)->vfs.handles.lgetxattr,(path),(name),(value),(size)))
#define SMB_VFS_FGETXATTR(fsp,fd,name,value,size) ((fsp)->conn->vfs.ops.fgetxattr((fsp)->conn->vfs.handles.fgetxattr,(fsp),(fd),(name),(value),(size)))
#define SMB_VFS_LISTXATTR(conn,path,list,size) ((conn)->vfs.ops.listxattr((conn)->vfs.handles.listxattr,(path),(list),(size)))
#define SMB_VFS_LLISTXATTR(conn,path,list,size) ((conn)->vfs.ops.llistxattr((conn)->vfs.handles.llistxattr,(path),(list),(size)))
#define SMB_VFS_FLISTXATTR(fsp,fd,list,size) ((fsp)->conn->vfs.ops.flistxattr((fsp)->conn->vfs.handles.flistxattr,(fsp),(fd),(list),(size)))
#define SMB_VFS_REMOVEXATTR(conn,path,name) ((conn)->vfs.ops.removexattr((conn)->vfs.handles.removexattr,(path),(name)))
#define SMB_VFS_LREMOVEXATTR(conn,path,name) ((conn)->vfs.ops.lremovexattr((conn)->vfs.handles.lremovexattr,(path),(name)))
#define SMB_VFS_FREMOVEXATTR(fsp,fd,name) ((fsp)->conn->vfs.ops.fremovexattr((fsp)->conn->vfs.handles.fremovexattr,(fsp),(fd),(name)))
#define SMB_VFS_SETXATTR(conn,path,name,value,size,flags) ((conn)->vfs.ops.setxattr((conn)->vfs.handles.setxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_LSETXATTR(conn,path,name,value,size,flags) ((conn)->vfs.ops.lsetxattr((conn)->vfs.handles.lsetxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_FSETXATTR(fsp,fd,name,value,size,flags) ((fsp)->conn->vfs.ops.fsetxattr((fsp)->conn->vfs.handles.fsetxattr,(fsp),(fd),(name),(value),(size),(flags)))

/* AIO operations. */
#define SMB_VFS_AIO_READ(fsp,aiocb) ((fsp)->conn->vfs.ops.aio_read((fsp)->conn->vfs.handles.aio_read,(fsp),(aiocb)))
#define SMB_VFS_AIO_WRITE(fsp,aiocb) ((fsp)->conn->vfs.ops.aio_write((fsp)->conn->vfs.handles.aio_write,(fsp),(aiocb)))
#define SMB_VFS_AIO_RETURN(fsp,aiocb) ((fsp)->conn->vfs.ops.aio_return_fn((fsp)->conn->vfs.handles.aio_return,(fsp),(aiocb)))
#define SMB_VFS_AIO_CANCEL(fsp,fd,aiocb) ((fsp)->conn->vfs.ops.aio_cancel((fsp)->conn->vfs.handles.aio_cancel,(fsp),(fd),(aiocb)))
#define SMB_VFS_AIO_ERROR(fsp,aiocb) ((fsp)->conn->vfs.ops.aio_error_fn((fsp)->conn->vfs.handles.aio_error,(fsp),(aiocb)))
#define SMB_VFS_AIO_FSYNC(fsp,op,aiocb) ((fsp)->conn->vfs.ops.aio_fsync((fsp)->conn->vfs.handles.aio_fsync,(fsp),(op),(aiocb)))
#define SMB_VFS_AIO_SUSPEND(fsp,aiocb,n,ts) ((fsp)->conn->vfs.ops.aio_suspend((fsp)->conn->vfs.handles.aio_suspend,(fsp),(aiocb),(n),(ts)))

/*******************************************************************
 Don't access conn->vfs_opaque.ops directly!!!
 Use this macros!
 (Fixes should also go into the vfs_* and vfs_next_* macros!)
********************************************************************/

/* Disk operations */    
#define SMB_VFS_OPAQUE_CONNECT(conn, service, user) ((conn)->vfs_opaque.ops.connect_fn((conn)->vfs_opaque.handles.connect_hnd, (service), (user)))
#define SMB_VFS_OPAQUE_DISCONNECT(conn) ((conn)->vfs_opaque.ops.disconnect((conn)->vfs_opaque.handles.disconnect))
#define SMB_VFS_OPAQUE_DISK_FREE(conn, path, small_query, bsize, dfree ,dsize) ((conn)->vfs_opaque.ops.disk_free((conn)->vfs_opaque.handles.disk_free, (path), (small_query), (bsize), (dfree), (dsize)))
#define SMB_VFS_OPAQUE_GET_QUOTA(conn, qtype, id, qt) ((conn)->vfs_opaque.ops.get_quota((conn)->vfs_opaque.handles.get_quota, (qtype), (id), (qt)))
#define SMB_VFS_OPAQUE_SET_QUOTA(conn, qtype, id, qt) ((conn)->vfs_opaque.ops.set_quota((conn)->vfs_opaque.handles.set_quota, (qtype), (id), (qt)))
#define SMB_VFS_OPAQUE_GET_SHADOW_COPY_DATA(fsp,shadow_copy_data,labels) ((fsp)->conn->vfs_opaque.ops.get_shadow_copy_data((fsp)->conn->vfs_opaque.handles.get_shadow_copy_data,(fsp),(shadow_copy_data),(labels)))
#define SMB_VFS_OPAQUE_STATVFS(conn, path, statbuf) ((conn)->vfs_opaque.ops.statvfs((conn)->vfs_opaque.handles.statvfs, (path), (statbuf)))

/* Directory operations */
#define SMB_VFS_OPAQUE_OPENDIR(conn, fname, mask, attr) ((conn)->vfs_opaque.ops.opendir((conn)->vfs_opaque.handles.opendir, (fname), (mask), (attr)))
#define SMB_VFS_OPAQUE_READDIR(conn, dirp) ((conn)->vfs_opaque.ops.readdir((conn)->vfs_opaque.handles.readdir, (dirp)))
#define SMB_VFS_OPAQUE_SEEKDIR(conn, dirp, offset) ((conn)->vfs_opaque.ops.seekdir((conn)->vfs_opaque.handles.seekdir, (dirp), (offset)))
#define SMB_VFS_OPAQUE_TELLDIR(conn, dirp) ((conn)->vfs_opaque.ops.telldir((conn)->vfs_opaque.handles.telldir, (dirp)))
#define SMB_VFS_OPAQUE_REWINDDIR(conn, dirp) ((conn)->vfs_opaque.ops.rewind_dir((conn)->vfs_opaque.handles.rewind_dir, (dirp)))
#define SMB_VFS_OPAQUE_MKDIR(conn, path, mode) ((conn)->vfs_opaque.ops.mkdir((conn)->vfs_opaque.handles.mkdir,(path), (mode)))
#define SMB_VFS_OPAQUE_RMDIR(conn, path) ((conn)->vfs_opaque.ops.rmdir((conn)->vfs_opaque.handles.rmdir, (path)))
#define SMB_VFS_OPAQUE_CLOSEDIR(conn, dir) ((conn)->vfs_opaque.ops.closedir((conn)->vfs_opaque.handles.closedir, dir))
    
/* File operations */
#define SMB_VFS_OPAQUE_OPEN(conn, fname, fsp, flags, mode) (((conn)->vfs_opaque.ops.open)((conn)->vfs_opaque.handles.open, (fname), (fsp), (flags), (mode)))
#define SMB_VFS_OPAQUE_CLOSE(fsp, fd) ((fsp)->conn->vfs_opaque.ops.close_fn((fsp)->conn->vfs_opaque.handles.close_hnd, (fsp), (fd)))
#define SMB_VFS_OPAQUE_READ(fsp, fd, data, n) ((fsp)->conn->vfs_opaque.ops.read((fsp)->conn->vfs_opaque.handles.read, (fsp), (fd), (data), (n)))
#define SMB_VFS_OPAQUE_PREAD(fsp, fd, data, n, off) ((fsp)->conn->vfs_opaque.ops.pread((fsp)->conn->vfs_opaque.handles.pread, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_OPAQUE_WRITE(fsp, fd, data, n) ((fsp)->conn->vfs_opaque.ops.write((fsp)->conn->vfs_opaque.handles.write, (fsp), (fd), (data), (n)))
#define SMB_VFS_OPAQUE_PWRITE(fsp, fd, data, n, off) ((fsp)->conn->vfs_opaque.ops.pwrite((fsp)->conn->vfs_opaque.handles.pwrite, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_OPAQUE_LSEEK(fsp, fd, offset, whence) ((fsp)->conn->vfs_opaque.ops.lseek((fsp)->conn->vfs_opaque.handles.lseek, (fsp), (fd), (offset), (whence)))
#define SMB_VFS_OPAQUE_SENDFILE(tofd, fsp, fromfd, header, offset, count) ((fsp)->conn->vfs_opaque.ops.sendfile((fsp)->conn->vfs_opaque.handles.sendfile, (tofd), (fsp), (fromfd), (header), (offset), (count)))
#define SMB_VFS_OPAQUE_RENAME(conn, old, new) ((conn)->vfs_opaque.ops.rename((conn)->vfs_opaque.handles.rename, (old), (new)))
#define SMB_VFS_OPAQUE_FSYNC(fsp, fd) ((fsp)->conn->vfs_opaque.ops.fsync((fsp)->conn->vfs_opaque.handles.fsync, (fsp), (fd)))
#define SMB_VFS_OPAQUE_STAT(conn, fname, sbuf) ((conn)->vfs_opaque.ops.stat((conn)->vfs_opaque.handles.stat, (fname), (sbuf)))
#define SMB_VFS_OPAQUE_FSTAT(fsp, fd, sbuf) ((fsp)->conn->vfs_opaque.ops.fstat((fsp)->conn->vfs_opaque.handles.fstat, (fsp) ,(fd) ,(sbuf)))
#define SMB_VFS_OPAQUE_LSTAT(conn, path, sbuf) ((conn)->vfs_opaque.ops.lstat((conn)->vfs_opaque.handles.lstat, (path), (sbuf)))
#define SMB_VFS_OPAQUE_UNLINK(conn, path) ((conn)->vfs_opaque.ops.unlink((conn)->vfs_opaque.handles.unlink, (path)))
#define SMB_VFS_OPAQUE_CHMOD(conn, path, mode) ((conn)->vfs_opaque.ops.chmod((conn)->vfs_opaque.handles.chmod, (path), (mode)))
#define SMB_VFS_OPAQUE_FCHMOD(fsp, fd, mode) ((fsp)->conn->vfs_opaque.ops.fchmod((fsp)->conn->vfs_opaque.handles.fchmod, (fsp), (fd), (mode)))
#define SMB_VFS_OPAQUE_CHOWN(conn, path, uid, gid) ((conn)->vfs_opaque.ops.chown((conn)->vfs_opaque.handles.chown, (path), (uid), (gid)))
#define SMB_VFS_OPAQUE_FCHOWN(fsp, fd, uid, gid) ((fsp)->conn->vfs_opaque.ops.fchown((fsp)->conn->vfs_opaque.handles.fchown, (fsp), (fd), (uid), (gid)))
#define SMB_VFS_OPAQUE_CHDIR(conn, path) ((conn)->vfs_opaque.ops.chdir((conn)->vfs_opaque.handles.chdir, (path)))
#define SMB_VFS_OPAQUE_GETWD(conn, buf) ((conn)->vfs_opaque.ops.getwd((conn)->vfs_opaque.handles.getwd, (buf)))
#define SMB_VFS_OPAQUE_NTIMES(conn, path, ts) ((conn)->vfs_opaque.ops.ntimes((conn)->vfs_opaque.handles.ntimes, (path), (ts)))
#define SMB_VFS_OPAQUE_FTRUNCATE(fsp, fd, offset) ((fsp)->conn->vfs_opaque.ops.ftruncate((fsp)->conn->vfs_opaque.handles.ftruncate, (fsp), (fd), (offset)))
#define SMB_VFS_OPAQUE_LOCK(fsp, fd, op, offset, count, type) ((fsp)->conn->vfs_opaque.ops.lock((fsp)->conn->vfs_opaque.handles.lock, (fsp), (fd) ,(op), (offset), (count), (type)))
#define SMB_VFS_OPAQUE_FLOCK(fsp, fd, share_mode) ((fsp)->conn->vfs_opaque.ops.lock((fsp)->conn->vfs_opaque.handles.kernel_flock, (fsp), (fd), (share_mode)))
#define SMB_VFS_OPAQUE_LINUX_SETLEASE(fsp, fd, leasetype) ((fsp)->conn->vfs_opaque.ops.linux_setlease((fsp)->conn->vfs_opaque.handles.linux_setlease, (fsp), (fd), (leasetype)))
#define SMB_VFS_OPAQUE_GETLOCK(fsp, fd, poffset, pcount, ptype, ppid) ((fsp)->conn->vfs_opaque.ops.getlock((fsp)->conn->vfs_opaque.handles.getlock, (fsp), (fd), (poffset), (pcount), (ptype), (ppid)))
#define SMB_VFS_OPAQUE_SYMLINK(conn, oldpath, newpath) ((conn)->vfs_opaque.ops.symlink((conn)->vfs_opaque.handles.symlink, (oldpath), (newpath)))
#define SMB_VFS_OPAQUE_READLINK(conn, path, buf, bufsiz) ((conn)->vfs_opaque.ops.readlink((conn)->vfs_opaque.handles.readlink, (path), (buf), (bufsiz)))
#define SMB_VFS_OPAQUE_LINK(conn, oldpath, newpath) ((conn)->vfs_opaque.ops.link((conn)->vfs_opaque.handles.link, (oldpath), (newpath)))
#define SMB_VFS_OPAQUE_MKNOD(conn, path, mode, dev) ((conn)->vfs_opaque.ops.mknod((conn)->vfs_opaque.handles.mknod, (path), (mode), (dev)))
#define SMB_VFS_OPAQUE_REALPATH(conn, path, resolved_path) ((conn)->vfs_opaque.ops.realpath((conn)->vfs_opaque.handles.realpath, (path), (resolved_path)))
#define SMB_VFS_OPAQUE_NOTIFY_WATCH(conn, ctx, e, callback, private_data, handle_p) ((conn)->vfs_opaque.ops.notify_watch((conn)->vfs_opaque.handles.notify_watch, (ctx), (e), (callback), (private_data), (handle_p)))
#define SMB_VFS_OPAQUE_CHFLAGS(conn, path, flags) ((conn)->vfs_opaque.ops.chflags((conn)->vfs_opaque.handles.chflags, (path), (flags)))

/* NT ACL operations. */
#define SMB_VFS_OPAQUE_FGET_NT_ACL(fsp, fd, security_info, ppdesc) ((fsp)->conn->vfs_opaque.ops.fget_nt_acl((fsp)->conn->vfs_opaque.handles.fget_nt_acl, (fsp), (fd), (security_info), (ppdesc)))
#define SMB_VFS_OPAQUE_GET_NT_ACL(fsp, name, security_info, ppdesc) ((fsp)->conn->vfs_opaque.ops.get_nt_acl((fsp)->conn->vfs_opaque.handles.get_nt_acl, (fsp), (name), (security_info), (ppdesc)))
#define SMB_VFS_OPAQUE_FSET_NT_ACL(fsp, fd, security_info_sent, psd) ((fsp)->conn->vfs_opaque.ops.fset_nt_acl((fsp)->conn->vfs_opaque.handles.fset_nt_acl, (fsp), (fd), (security_info_sent), (psd)))
#define SMB_VFS_OPAQUE_SET_NT_ACL(fsp, name, security_info_sent, psd) ((fsp)->conn->vfs_opaque.ops.set_nt_acl((fsp)->conn->vfs_opaque.handles.set_nt_acl, (fsp), (name), (security_info_sent), (psd)))

/* POSIX ACL operations. */
#define SMB_VFS_OPAQUE_CHMOD_ACL(conn, name, mode) ((conn)->vfs_opaque.ops.chmod_acl((conn)->vfs_opaque.handles.chmod_acl, (name), (mode)))
#define SMB_VFS_OPAQUE_FCHMOD_ACL(fsp, fd, mode) ((fsp)->conn->vfs_opaque.ops.fchmod_acl((fsp)->conn->vfs_opaque.handles.chmod_acl, (fsp), (fd), (mode)))

#define SMB_VFS_OPAQUE_SYS_ACL_GET_ENTRY(conn, theacl, entry_id, entry_p) ((conn)->vfs_opaque.ops.sys_acl_get_entry((conn)->vfs_opaque.handles.sys_acl_get_entry, (theacl), (entry_id), (entry_p)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_TAG_TYPE(conn, entry_d, tag_type_p) ((conn)->vfs_opaque.ops.sys_acl_get_tag_type((conn)->vfs_opaque.handles.sys_acl_get_tag_type, (entry_d), (tag_type_p)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_PERMSET(conn, entry_d, permset_p) ((conn)->vfs_opaque.ops.sys_acl_get_permset((conn)->vfs_opaque.handles.sys_acl_get_permset, (entry_d), (permset_p)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_QUALIFIER(conn, entry_d) ((conn)->vfs_opaque.ops.sys_acl_get_qualifier((conn)->vfs_opaque.handles.sys_acl_get_qualifier, (entry_d)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_FILE(conn, path_p, type) ((conn)->vfs_opaque.ops.sys_acl_get_file((conn)->vfs_opaque.handles.sys_acl_get_file, (path_p), (type)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_FD(fsp, fd) ((fsp)->conn->vfs_opaque.ops.sys_acl_get_fd((fsp)->conn->vfs_opaque.handles.sys_acl_get_fd, (fsp), (fd)))
#define SMB_VFS_OPAQUE_SYS_ACL_CLEAR_PERMS(conn, permset) ((conn)->vfs_opaque.ops.sys_acl_clear_perms((conn)->vfs_opaque.handles.sys_acl_clear_perms, (permset)))
#define SMB_VFS_OPAQUE_SYS_ACL_ADD_PERM(conn, permset, perm) ((conn)->vfs_opaque.ops.sys_acl_add_perm((conn)->vfs_opaque.handles.sys_acl_add_perm, (permset), (perm)))
#define SMB_VFS_OPAQUE_SYS_ACL_TO_TEXT(conn, theacl, plen) ((conn)->vfs_opaque.ops.sys_acl_to_text((conn)->vfs_opaque.handles.sys_acl_to_text, (theacl), (plen)))
#define SMB_VFS_OPAQUE_SYS_ACL_INIT(conn, count) ((conn)->vfs_opaque.ops.sys_acl_init((conn)->vfs_opaque.handles.sys_acl_init, (count)))
#define SMB_VFS_OPAQUE_SYS_ACL_CREATE_ENTRY(conn, pacl, pentry) ((conn)->vfs_opaque.ops.sys_acl_create_entry((conn)->vfs_opaque.handles.sys_acl_create_entry, (pacl), (pentry)))
#define SMB_VFS_OPAQUE_SYS_ACL_SET_TAG_TYPE(conn, entry, tagtype) ((conn)->vfs_opaque.ops.sys_acl_set_tag_type((conn)->vfs_opaque.handles.sys_acl_set_tag_type, (entry), (tagtype)))
#define SMB_VFS_OPAQUE_SYS_ACL_SET_QUALIFIER(conn, entry, qual) ((conn)->vfs_opaque.ops.sys_acl_set_qualifier((conn)->vfs_opaque.handles.sys_acl_set_qualifier, (entry), (qual)))
#define SMB_VFS_OPAQUE_SYS_ACL_SET_PERMSET(conn, entry, permset) ((conn)->vfs_opaque.ops.sys_acl_set_permset((conn)->vfs_opaque.handles.sys_acl_set_permset, (entry), (permset)))
#define SMB_VFS_OPAQUE_SYS_ACL_VALID(conn, theacl) ((conn)->vfs_opaque.ops.sys_acl_valid((conn)->vfs_opaque.handles.sys_acl_valid, (theacl)))
#define SMB_VFS_OPAQUE_SYS_ACL_SET_FILE(conn, name, acltype, theacl) ((conn)->vfs_opaque.ops.sys_acl_set_file((conn)->vfs_opaque.handles.sys_acl_set_file, (name), (acltype), (theacl)))
#define SMB_VFS_OPAQUE_SYS_ACL_SET_FD(fsp, fd, theacl) ((fsp)->conn->vfs_opaque.ops.sys_acl_set_fd((fsp)->conn->vfs_opaque.handles.sys_acl_set_fd, (fsp), (fd), (theacl)))
#define SMB_VFS_OPAQUE_SYS_ACL_DELETE_DEF_FILE(conn, path) ((conn)->vfs_opaque.ops.sys_acl_delete_def_file((conn)->vfs_opaque.handles.sys_acl_delete_def_file, (path)))
#define SMB_VFS_OPAQUE_SYS_ACL_GET_PERM(conn, permset, perm) ((conn)->vfs_opaque.ops.sys_acl_get_perm((conn)->vfs_opaque.handles.sys_acl_get_perm, (permset), (perm)))
#define SMB_VFS_OPAQUE_SYS_ACL_FREE_TEXT(conn, text) ((conn)->vfs_opaque.ops.sys_acl_free_text((conn)->vfs_opaque.handles.sys_acl_free_text, (text)))
#define SMB_VFS_OPAQUE_SYS_ACL_FREE_ACL(conn, posix_acl) ((conn)->vfs_opaque.ops.sys_acl_free_acl((conn)->vfs_opaque.handles.sys_acl_free_acl, (posix_acl)))
#define SMB_VFS_OPAQUE_SYS_ACL_FREE_QUALIFIER(conn, qualifier, tagtype) ((conn)->vfs_opaque.ops.sys_acl_free_qualifier((conn)->vfs_opaque.handles.sys_acl_free_qualifier, (qualifier), (tagtype)))

/* EA operations. */
#define SMB_VFS_OPAQUE_GETXATTR(conn,path,name,value,size) ((conn)->vfs_opaque.ops.getxattr((conn)->vfs_opaque.handles.getxattr,(path),(name),(value),(size)))
#define SMB_VFS_OPAQUE_LGETXATTR(conn,path,name,value,size) ((conn)->vfs_opaque.ops.lgetxattr((conn)->vfs_opaque.handles.lgetxattr,(path),(name),(value),(size)))
#define SMB_VFS_OPAQUE_FGETXATTR(fsp,fd,name,value,size) ((fsp)->conn->vfs_opaque.ops.fgetxattr((fsp)->conn->vfs_opaque.handles.fgetxattr,(fsp),(fd),(name),(value),(size)))
#define SMB_VFS_OPAQUE_LISTXATTR(conn,path,list,size) ((conn)->vfs_opaque.ops.listxattr((conn)->vfs_opaque.handles.listxattr,(path),(list),(size)))
#define SMB_VFS_OPAQUE_LLISTXATTR(conn,path,list,size) ((conn)->vfs_opaque.ops.llistxattr((conn)->vfs_opaque.handles.llistxattr,(path),(list),(size)))
#define SMB_VFS_OPAQUE_FLISTXATTR(fsp,fd,list,size) ((fsp)->conn->vfs_opaque.ops.flistxattr((fsp)->conn->vfs_opaque.handles.flistxattr,(fsp),(fd),(list),(size)))
#define SMB_VFS_OPAQUE_REMOVEXATTR(conn,path,name) ((conn)->vfs_opaque.ops.removexattr((conn)->vfs_opaque.handles.removexattr,(path),(name)))
#define SMB_VFS_OPAQUE_LREMOVEXATTR(conn,path,name) ((conn)->vfs_opaque.ops.lremovexattr((conn)->vfs_opaque.handles.lremovexattr,(path),(name)))
#define SMB_VFS_OPAQUE_FREMOVEXATTR(fsp,fd,name) ((fsp)->conn->vfs_opaque.ops.fremovexattr((fsp)->conn->vfs_opaque.handles.fremovexattr,(fsp),(fd),(name)))
#define SMB_VFS_OPAQUE_SETXATTR(conn,path,name,value,size,flags) ((conn)->vfs_opaque.ops.setxattr((conn)->vfs_opaque.handles.setxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_OPAQUE_LSETXATTR(conn,path,name,value,size,flags) ((conn)->vfs_opaque.ops.lsetxattr((conn)->vfs_opaque.handles.lsetxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_OPAQUE_FSETXATTR(fsp,fd,name,value,size,flags) ((fsp)->conn->vfs_opaque.ops.fsetxattr((fsp)->conn->vfs_opaque.handles.fsetxattr,(fsp),(fd),(name),(value),(size),(flags)))

/* AIO operations. */
#define SMB_VFS_OPAQUE_AIO_READ(fsp,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_read((fsp)->conn->vfs_opaque.handles.aio_read,(fsp),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_WRITE(fsp,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_write((fsp)->conn->vfs_opaque.handles.aio_write,(fsp),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_RETURN(fsp,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_return_fn((fsp)->conn->vfs_opaque.handles.aio_return,(fsp),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_CANCEL(fsp,fd,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_cancel((fsp)->conn->vfs_opaque.handles.cancel,(fsp),(fd),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_ERROR(fsp,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_error_fn((fsp)->conn->vfs_opaque.handles.aio_error,(fsp),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_FSYNC(fsp,op,aiocb) ((fsp)->conn->vfs_opaque.ops.aio_fsync((fsp)->conn->vfs_opaque.handles.aio_fsync,(fsp),(op),(aiocb)))
#define SMB_VFS_OPAQUE_AIO_SUSPEND(fsp,aiocb,n,ts) ((fsp)->conn->vfs_opaque.ops.aio_suspend((fsp)->conn->vfs_opaque.handles.aio_suspend,(fsp),(aiocb),(n),(ts)))

/*******************************************************************
 Don't access handle->vfs_next.ops.* directly!!!
 Use this macros!
 (Fixes should go also into the vfs_* and vfs_opaque_* macros!)
********************************************************************/

/* Disk operations */    
#define SMB_VFS_NEXT_CONNECT(handle, service, user) ((handle)->vfs_next.ops.connect_fn((handle)->vfs_next.handles.connect_hnd, (service), (user)))
#define SMB_VFS_NEXT_DISCONNECT(handle) ((handle)->vfs_next.ops.disconnect((handle)->vfs_next.handles.disconnect))
#define SMB_VFS_NEXT_DISK_FREE(handle, path, small_query, bsize, dfree ,dsize) ((handle)->vfs_next.ops.disk_free((handle)->vfs_next.handles.disk_free, (path), (small_query), (bsize), (dfree), (dsize)))
#define SMB_VFS_NEXT_GET_QUOTA(handle, qtype, id, qt) ((handle)->vfs_next.ops.get_quota((handle)->vfs_next.handles.get_quota, (qtype), (id), (qt)))
#define SMB_VFS_NEXT_SET_QUOTA(handle, qtype, id, qt) ((handle)->vfs_next.ops.set_quota((handle)->vfs_next.handles.set_quota, (qtype), (id), (qt)))
#define SMB_VFS_NEXT_GET_SHADOW_COPY_DATA(handle, fsp, shadow_copy_data ,labels) ((handle)->vfs_next.ops.get_shadow_copy_data((handle)->vfs_next.handles.get_shadow_copy_data,(fsp),(shadow_copy_data),(labels)))
#define SMB_VFS_NEXT_STATVFS(handle, path, statbuf) ((handle)->vfs_next.ops.statvfs((handle)->vfs_next.handles.statvfs, (path), (statbuf)))

/* Directory operations */
#define SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr) ((handle)->vfs_next.ops.opendir((handle)->vfs_next.handles.opendir, (fname), (mask), (attr)))
#define SMB_VFS_NEXT_READDIR(handle, dirp) ((handle)->vfs_next.ops.readdir((handle)->vfs_next.handles.readdir, (dirp)))
#define SMB_VFS_NEXT_SEEKDIR(handle, dirp, offset) ((handle)->vfs_next.ops.seekdir((handle)->vfs_next.handles.seekdir, (dirp), (offset)))
#define SMB_VFS_NEXT_TELLDIR(handle, dirp) ((handle)->vfs_next.ops.telldir((handle)->vfs_next.handles.telldir, (dirp)))
#define SMB_VFS_NEXT_REWINDDIR(handle, dirp) ((handle)->vfs_next.ops.rewind_dir((handle)->vfs_next.handles.rewind_dir, (dirp)))
#define SMB_VFS_NEXT_DIR(handle, dirp) ((handle)->vfs_next.ops.readdir((handle)->vfs_next.handles.readdir, (dirp)))
#define SMB_VFS_NEXT_MKDIR(handle, path, mode) ((handle)->vfs_next.ops.mkdir((handle)->vfs_next.handles.mkdir,(path), (mode)))
#define SMB_VFS_NEXT_RMDIR(handle, path) ((handle)->vfs_next.ops.rmdir((handle)->vfs_next.handles.rmdir, (path)))
#define SMB_VFS_NEXT_CLOSEDIR(handle, dir) ((handle)->vfs_next.ops.closedir((handle)->vfs_next.handles.closedir, dir))
    
/* File operations */
#define SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode) (((handle)->vfs_next.ops.open)((handle)->vfs_next.handles.open, (fname), (fsp), (flags), (mode)))
#define SMB_VFS_NEXT_CLOSE(handle, fsp, fd) ((handle)->vfs_next.ops.close_fn((handle)->vfs_next.handles.close_hnd, (fsp), (fd)))
#define SMB_VFS_NEXT_READ(handle, fsp, fd, data, n) ((handle)->vfs_next.ops.read((handle)->vfs_next.handles.read, (fsp), (fd), (data), (n)))
#define SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, n, off) ((handle)->vfs_next.ops.pread((handle)->vfs_next.handles.pread, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_NEXT_WRITE(handle, fsp, fd, data, n) ((handle)->vfs_next.ops.write((handle)->vfs_next.handles.write, (fsp), (fd), (data), (n)))
#define SMB_VFS_NEXT_PWRITE(handle, fsp, fd, data, n, off) ((handle)->vfs_next.ops.pwrite((handle)->vfs_next.handles.pwrite, (fsp), (fd), (data), (n), (off)))
#define SMB_VFS_NEXT_LSEEK(handle, fsp, fd, offset, whence) ((handle)->vfs_next.ops.lseek((handle)->vfs_next.handles.lseek, (fsp), (fd), (offset), (whence)))
#define SMB_VFS_NEXT_SENDFILE(handle, tofd, fsp, fromfd, header, offset, count) ((handle)->vfs_next.ops.sendfile((handle)->vfs_next.handles.sendfile, (tofd), (fsp), (fromfd), (header), (offset), (count)))
#define SMB_VFS_NEXT_RENAME(handle, old, new) ((handle)->vfs_next.ops.rename((handle)->vfs_next.handles.rename, (old), (new)))
#define SMB_VFS_NEXT_FSYNC(handle, fsp, fd) ((handle)->vfs_next.ops.fsync((handle)->vfs_next.handles.fsync, (fsp), (fd)))
#define SMB_VFS_NEXT_STAT(handle, fname, sbuf) ((handle)->vfs_next.ops.stat((handle)->vfs_next.handles.stat, (fname), (sbuf)))
#define SMB_VFS_NEXT_FSTAT(handle, fsp, fd, sbuf) ((handle)->vfs_next.ops.fstat((handle)->vfs_next.handles.fstat, (fsp) ,(fd) ,(sbuf)))
#define SMB_VFS_NEXT_LSTAT(handle, path, sbuf) ((handle)->vfs_next.ops.lstat((handle)->vfs_next.handles.lstat, (path), (sbuf)))
#define SMB_VFS_NEXT_UNLINK(handle, path) ((handle)->vfs_next.ops.unlink((handle)->vfs_next.handles.unlink, (path)))
#define SMB_VFS_NEXT_CHMOD(handle, path, mode) ((handle)->vfs_next.ops.chmod((handle)->vfs_next.handles.chmod, (path), (mode)))
#define SMB_VFS_NEXT_FCHMOD(handle, fsp, fd, mode) ((handle)->vfs_next.ops.fchmod((handle)->vfs_next.handles.fchmod, (fsp), (fd), (mode)))
#define SMB_VFS_NEXT_CHOWN(handle, path, uid, gid) ((handle)->vfs_next.ops.chown((handle)->vfs_next.handles.chown, (path), (uid), (gid)))
#define SMB_VFS_NEXT_FCHOWN(handle, fsp, fd, uid, gid) ((handle)->vfs_next.ops.fchown((handle)->vfs_next.handles.fchown, (fsp), (fd), (uid), (gid)))
#define SMB_VFS_NEXT_CHDIR(handle, path) ((handle)->vfs_next.ops.chdir((handle)->vfs_next.handles.chdir, (path)))
#define SMB_VFS_NEXT_GETWD(handle, buf) ((handle)->vfs_next.ops.getwd((handle)->vfs_next.handles.getwd, (buf)))
#define SMB_VFS_NEXT_NTIMES(handle, path, ts) ((handle)->vfs_next.ops.ntimes((handle)->vfs_next.handles.ntimes, (path), (ts)))
#define SMB_VFS_NEXT_FTRUNCATE(handle, fsp, fd, offset) ((handle)->vfs_next.ops.ftruncate((handle)->vfs_next.handles.ftruncate, (fsp), (fd), (offset)))
#define SMB_VFS_NEXT_LOCK(handle, fsp, fd, op, offset, count, type) ((handle)->vfs_next.ops.lock((handle)->vfs_next.handles.lock, (fsp), (fd) ,(op), (offset), (count), (type)))
#define SMB_VFS_NEXT_KERNEL_FLOCK(handle, fsp, fd, share_mode)((handle)->vfs_next.ops.kernel_flock((handle)->vfs_next.handles.kernel_flock, (fsp), (fd), (share_mode)))
#define SMB_VFS_NEXT_LINUX_SETLEASE(handle, fsp, fd, leasetype)((handle)->vfs_next.ops.linux_setlease((handle)->vfs_next.handles.linux_setlease, (fsp), (fd), (leasetype)))
#define SMB_VFS_NEXT_GETLOCK(handle, fsp, fd, poffset, pcount, ptype, ppid) ((handle)->vfs_next.ops.getlock((handle)->vfs_next.handles.getlock, (fsp), (fd), (poffset), (pcount), (ptype), (ppid)))
#define SMB_VFS_NEXT_SYMLINK(handle, oldpath, newpath) ((handle)->vfs_next.ops.symlink((handle)->vfs_next.handles.symlink, (oldpath), (newpath)))
#define SMB_VFS_NEXT_READLINK(handle, path, buf, bufsiz) ((handle)->vfs_next.ops.readlink((handle)->vfs_next.handles.readlink, (path), (buf), (bufsiz)))
#define SMB_VFS_NEXT_LINK(handle, oldpath, newpath) ((handle)->vfs_next.ops.link((handle)->vfs_next.handles.link, (oldpath), (newpath)))
#define SMB_VFS_NEXT_MKNOD(handle, path, mode, dev) ((handle)->vfs_next.ops.mknod((handle)->vfs_next.handles.mknod, (path), (mode), (dev)))
#define SMB_VFS_NEXT_REALPATH(handle, path, resolved_path) ((handle)->vfs_next.ops.realpath((handle)->vfs_next.handles.realpath, (path), (resolved_path)))
#define SMB_VFS_NEXT_NOTIFY_WATCH(conn, ctx, e, callback, private_data, handle_p) ((conn)->vfs_next.ops.notify_watch((conn)->vfs_next.handles.notify_watch, (ctx), (e), (callback), (private_data), (handle_p)))
#define SMB_VFS_NEXT_CHFLAGS(handle, path, flags) ((handle)->vfs_next.ops.chflags((handle)->vfs_next.handles.chflags, (path), (flags)))

/* NT ACL operations. */
#define SMB_VFS_NEXT_FGET_NT_ACL(handle, fsp, fd, security_info, ppdesc) ((handle)->vfs_next.ops.fget_nt_acl((handle)->vfs_next.handles.fget_nt_acl, (fsp), (fd), (security_info), (ppdesc)))
#define SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, name, security_info, ppdesc) ((handle)->vfs_next.ops.get_nt_acl((handle)->vfs_next.handles.get_nt_acl, (fsp), (name), (security_info), (ppdesc)))
#define SMB_VFS_NEXT_FSET_NT_ACL(handle, fsp, fd, security_info_sent, psd) ((handle)->vfs_next.ops.fset_nt_acl((handle)->vfs_next.handles.fset_nt_acl, (fsp), (fd), (security_info_sent), (psd)))
#define SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, name, security_info_sent, psd) ((handle)->vfs_next.ops.set_nt_acl((handle)->vfs_next.handles.set_nt_acl, (fsp), (name), (security_info_sent), (psd)))

/* POSIX ACL operations. */
#define SMB_VFS_NEXT_CHMOD_ACL(handle, name, mode) ((handle)->vfs_next.ops.chmod_acl((handle)->vfs_next.handles.chmod_acl, (name), (mode)))
#define SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, fd, mode) ((handle)->vfs_next.ops.fchmod_acl((handle)->vfs_next.handles.chmod_acl, (fsp), (fd), (mode)))

#define SMB_VFS_NEXT_SYS_ACL_GET_ENTRY(handle, theacl, entry_id, entry_p) ((handle)->vfs_next.ops.sys_acl_get_entry((handle)->vfs_next.handles.sys_acl_get_entry, (theacl), (entry_id), (entry_p)))
#define SMB_VFS_NEXT_SYS_ACL_GET_TAG_TYPE(handle, entry_d, tag_type_p) ((handle)->vfs_next.ops.sys_acl_get_tag_type((handle)->vfs_next.handles.sys_acl_get_tag_type, (entry_d), (tag_type_p)))
#define SMB_VFS_NEXT_SYS_ACL_GET_PERMSET(handle, entry_d, permset_p) ((handle)->vfs_next.ops.sys_acl_get_permset((handle)->vfs_next.handles.sys_acl_get_permset, (entry_d), (permset_p)))
#define SMB_VFS_NEXT_SYS_ACL_GET_QUALIFIER(handle, entry_d) ((handle)->vfs_next.ops.sys_acl_get_qualifier((handle)->vfs_next.handles.sys_acl_get_qualifier, (entry_d)))
#define SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, path_p, type) ((handle)->vfs_next.ops.sys_acl_get_file((handle)->vfs_next.handles.sys_acl_get_file, (path_p), (type)))
#define SMB_VFS_NEXT_SYS_ACL_GET_FD(handle, fsp, fd) ((handle)->vfs_next.ops.sys_acl_get_fd((handle)->vfs_next.handles.sys_acl_get_fd, (fsp), (fd)))
#define SMB_VFS_NEXT_SYS_ACL_CLEAR_PERMS(handle, permset) ((handle)->vfs_next.ops.sys_acl_clear_perms((handle)->vfs_next.handles.sys_acl_clear_perms, (permset)))
#define SMB_VFS_NEXT_SYS_ACL_ADD_PERM(handle, permset, perm) ((handle)->vfs_next.ops.sys_acl_add_perm((handle)->vfs_next.handles.sys_acl_add_perm, (permset), (perm)))
#define SMB_VFS_NEXT_SYS_ACL_TO_TEXT(handle, theacl, plen) ((handle)->vfs_next.ops.sys_acl_to_text((handle)->vfs_next.handles.sys_acl_to_text, (theacl), (plen)))
#define SMB_VFS_NEXT_SYS_ACL_INIT(handle, count) ((handle)->vfs_next.ops.sys_acl_init((handle)->vfs_next.handles.sys_acl_init, (count)))
#define SMB_VFS_NEXT_SYS_ACL_CREATE_ENTRY(handle, pacl, pentry) ((handle)->vfs_next.ops.sys_acl_create_entry((handle)->vfs_next.handles.sys_acl_create_entry, (pacl), (pentry)))
#define SMB_VFS_NEXT_SYS_ACL_SET_TAG_TYPE(handle, entry, tagtype) ((handle)->vfs_next.ops.sys_acl_set_tag_type((handle)->vfs_next.handles.sys_acl_set_tag_type, (entry), (tagtype)))
#define SMB_VFS_NEXT_SYS_ACL_SET_QUALIFIER(handle, entry, qual) ((handle)->vfs_next.ops.sys_acl_set_qualifier((handle)->vfs_next.handles.sys_acl_set_qualifier, (entry), (qual)))
#define SMB_VFS_NEXT_SYS_ACL_SET_PERMSET(handle, entry, permset) ((handle)->vfs_next.ops.sys_acl_set_permset((handle)->vfs_next.handles.sys_acl_set_permset, (entry), (permset)))
#define SMB_VFS_NEXT_SYS_ACL_VALID(handle, theacl) ((handle)->vfs_next.ops.sys_acl_valid((handle)->vfs_next.handles.sys_acl_valid, (theacl)))
#define SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, name, acltype, theacl) ((handle)->vfs_next.ops.sys_acl_set_file((handle)->vfs_next.handles.sys_acl_set_file, (name), (acltype), (theacl)))
#define SMB_VFS_NEXT_SYS_ACL_SET_FD(handle, fsp, fd, theacl) ((handle)->vfs_next.ops.sys_acl_set_fd((handle)->vfs_next.handles.sys_acl_set_fd, (fsp), (fd), (theacl)))
#define SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, path) ((handle)->vfs_next.ops.sys_acl_delete_def_file((handle)->vfs_next.handles.sys_acl_delete_def_file, (path)))
#define SMB_VFS_NEXT_SYS_ACL_GET_PERM(handle, permset, perm) ((handle)->vfs_next.ops.sys_acl_get_perm((handle)->vfs_next.handles.sys_acl_get_perm, (permset), (perm)))
#define SMB_VFS_NEXT_SYS_ACL_FREE_TEXT(handle, text) ((handle)->vfs_next.ops.sys_acl_free_text((handle)->vfs_next.handles.sys_acl_free_text, (text)))
#define SMB_VFS_NEXT_SYS_ACL_FREE_ACL(handle, posix_acl) ((handle)->vfs_next.ops.sys_acl_free_acl((handle)->vfs_next.handles.sys_acl_free_acl, (posix_acl)))
#define SMB_VFS_NEXT_SYS_ACL_FREE_QUALIFIER(handle, qualifier, tagtype) ((handle)->vfs_next.ops.sys_acl_free_qualifier((handle)->vfs_next.handles.sys_acl_free_qualifier, (qualifier), (tagtype)))

/* EA operations. */
#define SMB_VFS_NEXT_GETXATTR(handle,path,name,value,size) ((handle)->vfs_next.ops.getxattr((handle)->vfs_next.handles.getxattr,(path),(name),(value),(size)))
#define SMB_VFS_NEXT_LGETXATTR(handle,path,name,value,size) ((handle)->vfs_next.ops.lgetxattr((handle)->vfs_next.handles.lgetxattr,(path),(name),(value),(size)))
#define SMB_VFS_NEXT_FGETXATTR(handle,fsp,fd,name,value,size) ((handle)->vfs_next.ops.fgetxattr((handle)->vfs_next.handles.fgetxattr,(fsp),(fd),(name),(value),(size)))
#define SMB_VFS_NEXT_LISTXATTR(handle,path,list,size) ((handle)->vfs_next.ops.listxattr((handle)->vfs_next.handles.listxattr,(path),(list),(size)))
#define SMB_VFS_NEXT_LLISTXATTR(handle,path,list,size) ((handle)->vfs_next.ops.llistxattr((handle)->vfs_next.handles.llistxattr,(path),(list),(size)))
#define SMB_VFS_NEXT_FLISTXATTR(handle,fsp,fd,list,size) ((handle)->vfs_next.ops.flistxattr((handle)->vfs_next.handles.flistxattr,(fsp),(fd),(list),(size)))
#define SMB_VFS_NEXT_REMOVEXATTR(handle,path,name) ((handle)->vfs_next.ops.removexattr((handle)->vfs_next.handles.removexattr,(path),(name)))
#define SMB_VFS_NEXT_LREMOVEXATTR(handle,path,name) ((handle)->vfs_next.ops.lremovexattr((handle)->vfs_next.handles.lremovexattr,(path),(name)))
#define SMB_VFS_NEXT_FREMOVEXATTR(handle,fsp,fd,name) ((handle)->vfs_next.ops.fremovexattr((handle)->vfs_next.handles.fremovexattr,(fsp),(fd),(name)))
#define SMB_VFS_NEXT_SETXATTR(handle,path,name,value,size,flags) ((handle)->vfs_next.ops.setxattr((handle)->vfs_next.handles.setxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_NEXT_LSETXATTR(handle,path,name,value,size,flags) ((handle)->vfs_next.ops.lsetxattr((handle)->vfs_next.handles.lsetxattr,(path),(name),(value),(size),(flags)))
#define SMB_VFS_NEXT_FSETXATTR(handle,fsp,fd,name,value,size,flags) ((handle)->vfs_next.ops.fsetxattr((handle)->vfs_next.handles.fsetxattr,(fsp),(fd),(name),(value),(size),(flags)))

/* AIO operations. */
#define SMB_VFS_NEXT_AIO_READ(handle,fsp,aiocb) ((handle)->vfs_next.ops.aio_read((handle)->vfs_next.handles.aio_read,(fsp),(aiocb)))
#define SMB_VFS_NEXT_AIO_WRITE(handle,fsp,aiocb) ((handle)->vfs_next.ops.aio_write((handle)->vfs_next.handles.aio_write,(fsp),(aiocb)))
#define SMB_VFS_NEXT_AIO_RETURN(handle,fsp,aiocb) ((handle)->vfs_next.ops.aio_return_fn((handle)->vfs_next.handles.aio_return,(fsp),(aiocb)))
#define SMB_VFS_NEXT_AIO_CANCEL(handle,fsp,fd,aiocb) ((handle)->vfs_next.ops.aio_cancel((handle)->vfs_next.handles.aio_cancel,(fsp),(fd),(aiocb)))
#define SMB_VFS_NEXT_AIO_ERROR(handle,fsp,aiocb) ((handle)->vfs_next.ops.aio_error_fn((handle)->vfs_next.handles.aio_error,(fsp),(aiocb)))
#define SMB_VFS_NEXT_AIO_FSYNC(handle,fsp,op,aiocb) ((handle)->vfs_next.ops.aio_fsync((handle)->vfs_next.handles.aio_fsync,(fsp),(op),(aiocb)))
#define SMB_VFS_NEXT_AIO_SUSPEND(handle,fsp,aiocb,n,ts) ((handle)->vfs_next.ops.aio_suspend((handle)->vfs_next.handles.aio_suspend,(fsp),(aiocb),(n),(ts)))

#endif /* _VFS_MACROS_H */
