/* 
   Unix SMB/CIFS implementation.
   VFS structures and parameters
   Copyright (C) Jeremy Allison                         1999-2005
   Copyright (C) Tim Potter				1999
   Copyright (C) Alexander Bokovoy			2002-2005
   Copyright (C) Stefan (metze) Metzmacher		2003
   Copyright (C) Volker Lendecke			2009
   
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

   This work was sponsored by Optifacio Software Services, Inc.
*/

#ifndef _VFS_H
#define _VFS_H

/* Avoid conflict with an AIX include file */

#ifdef vfs_ops
#undef vfs_ops
#endif

/*
 * As we're now (thanks Andrew ! :-) using file_structs and connection
 * structs in the vfs - then anyone writing a vfs must include includes.h...
 */

/*
 * This next constant specifies the version number of the VFS interface
 * this smbd will load. Increment this if *ANY* changes are made to the
 * vfs_ops below. JRA.
 *
 * If you change anything here, please also update modules/vfs_full_audit.c.
 * VL.
 */

/* Changed to version 2 for CIFS UNIX extensions (mknod and link added). JRA. */
/* Changed to version 3 for POSIX acl extensions. JRA. */
/* Changed to version 4 for cascaded VFS interface. Alexander Bokovoy. */
/* Changed to version 5 for sendfile addition. JRA. */
/* Changed to version 6 for the new module system, fixed cascading and quota functions. --metze */
/* Changed to version 7 to include the get_nt_acl info parameter. JRA. */
/* Changed to version 8 includes EA calls. JRA. */
/* Changed to version 9 to include the get_shadow_data call. --metze */
/* Changed to version 10 to include pread/pwrite calls. */
/* Changed to version 11 to include seekdir/telldir/rewinddir calls. JRA */
/* Changed to version 12 to add mask and attributes to opendir(). JRA 
   Also include aio calls. JRA. */
/* Changed to version 13 as the internal structure of files_struct has changed. JRA */
/* Changed to version 14 as we had to change DIR to SMB_STRUCT_DIR. JRA */
/* Changed to version 15 as we added the statvfs call. JRA */
/* Changed to version 16 as we added the getlock call. JRA */
/* Changed to version 17 as we removed redundant connection_struct parameters. --jpeach */
/* Changed to version 18 to add fsp parameter to the open call -- jpeach 
   Also include kernel_flock call - jmcd */
/* Changed to version 19, kernel change notify has been merged 
   Also included linux setlease call - jmcd */
/* Changed to version 20, use ntimes call instead of utime (greater
 * timestamp resolition. JRA. */
/* Changed to version21 to add chflags operation -- jpeach */
/* Changed to version22 to add lchown operation -- jra */
/* Leave at 22 - not yet released. But change set_nt_acl to return an NTSTATUS. jra. */
/* Leave at 22 - not yet released. Add file_id_create operation. --metze */
/* Leave at 22 - not yet released. Change all BOOL parameters (int) to bool. jra. */
/* Leave at 22 - not yet released. Added recvfile. */
/* Leave at 22 - not yet released. Change get_nt_acl to return NTSTATUS - vl */
/* Leave at 22 - not yet released. Change get_nt_acl to *not* take a
 * files_struct. - obnox.*/
/* Leave at 22 - not yet released. Remove parameter fd from fget_nt_acl. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from gset_nt_acl. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from pread. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from pwrite. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from lseek. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fsync. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fstat. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fchmod. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fchown. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from ftruncate. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from lock. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from kernel_flock. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from linux_setlease. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from getlock. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from sys_acl_get_fd. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fchmod_acl. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from sys_acl_set_fd. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fgetxattr. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from flistxattr. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fremovexattr. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from fsetxattr. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from aio_cancel. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from read. - obnox */
/* Leave at 22 - not yet released. Remove parameter fd from write. - obnox */
/* Leave at 22 - not yet released. Remove parameter fromfd from sendfile. - obnox */
/* Leave at 22 - not yet released. Remove parameter fromfd from recvfile. - obnox */
/* Leave at 22 - not yet released. Additional change: add operations for offline files -- ab */
/* Leave at 22 - not yet released. Add the streaminfo call. -- jpeach, vl */
/* Leave at 22 - not yet released. Remove parameter fd from close_fn. - obnox */
/* Changed to version 23 - remove set_nt_acl call. This can only be done via an
   open handle. JRA. */
/* Changed to version 24 - make security descriptor const in fset_nt_acl. JRA. */
/* Changed to version 25 - Jelmer's change from SMB_BIG_UINT to uint64_t. */
/* Leave at 25 - not yet released. Add create_file call. -- tprouty. */
/* Leave at 25 - not yet released. Add create time to ntimes. -- tstecher. */
/* Leave at 25 - not yet released. Add get_alloc_size call. -- tprouty. */
/* Leave at 25 - not yet released. Add SMB_STRUCT_STAT to readdir. - sdann */
/* Leave at 25 - not yet released. Add init_search_op call. - sdann */
/* Leave at 25 - not yet released. Add locking calls. -- zkirsch. */
/* Leave at 25 - not yet released. Add strict locking calls. -- drichards. */
/* Changed to version 26 - Plumb struct smb_filename to SMB_VFS_CREATE_FILE,
			   SMB_VFS_OPEN, SMB_VFS_STAT, SMB_VFS_LSTAT,
			   SMB_VFS_RENAME, SMB_VFS_UNLINK, SMB_VFS_NTIMES.  */
/* Changed to version 27 - not yet released. Added enum timestamp_set_resolution
 * 			   return to fs_capabilities call. JRA. */
/* Leave at 27 - not yet released. Add translate_name VFS call to convert
		 UNIX names to Windows supported names -- asrinivasan. */
/* Changed to version 28 - Add private_flags uint32_t to CREATE call. */
/* Leave at 28 - not yet released. Change realpath to assume NULL and return a
		 malloc'ed path. JRA. */
/* Leave at 28 - not yet released. Move posix_fallocate into the VFS
		where it belongs. JRA. */
/* Leave at 28 - not yet released. Rename posix_fallocate to fallocate
		to split out the two possible uses. JRA. */
/* Leave at 28 - not yet released. Add fdopendir. JRA. */
/* Leave at 28 - not yet released. Rename open function to open_fn. - gd */
#define SMB_VFS_INTERFACE_VERSION 28

/*
    All intercepted VFS operations must be declared as static functions inside module source
    in order to keep smbd namespace unpolluted. See source of audit, extd_audit, fake_perms and recycle
    example VFS modules for more details.
*/

/* VFS operations structure */

struct vfs_handle_struct;
struct connection_struct;
struct files_struct;
struct security_descriptor;
struct vfs_statvfs_struct;
struct smb_request;
struct ea_list;
struct smb_file_time;
struct blocking_lock_record;
struct smb_filename;

#define VFS_FIND(__fn__) while (handle->fns->__fn__==NULL) { \
				handle = handle->next; \
			 }

enum vfs_translate_direction {
	vfs_translate_to_unix = 0,
	vfs_translate_to_windows
};

enum vfs_fallocate_mode {
	VFS_FALLOCATE_EXTEND_SIZE = 0,
	VFS_FALLOCATE_KEEP_SIZE = 1
};

/*
    Available VFS operations. These values must be in sync with vfs_ops struct
    (struct vfs_fn_pointers and struct vfs_handle_pointers inside of struct vfs_ops).
    In particular, if new operations are added to vfs_ops, appropriate constants
    should be added to vfs_op_type so that order of them kept same as in vfs_ops.
*/
struct shadow_copy_data;

struct vfs_fn_pointers {
	/* Disk operations */

	int (*connect_fn)(struct vfs_handle_struct *handle, const char *service, const char *user);
	void (*disconnect)(struct vfs_handle_struct *handle);
	uint64_t (*disk_free)(struct vfs_handle_struct *handle, const char *path, bool small_query, uint64_t *bsize,
			      uint64_t *dfree, uint64_t *dsize);
	int (*get_quota)(struct vfs_handle_struct *handle, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *qt);
	int (*set_quota)(struct vfs_handle_struct *handle, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *qt);
	int (*get_shadow_copy_data)(struct vfs_handle_struct *handle, struct files_struct *fsp, struct shadow_copy_data *shadow_copy_data, bool labels);
	int (*statvfs)(struct vfs_handle_struct *handle, const char *path, struct vfs_statvfs_struct *statbuf);
	uint32_t (*fs_capabilities)(struct vfs_handle_struct *handle, enum timestamp_set_resolution *p_ts_res);

	/* Directory operations */

	SMB_STRUCT_DIR *(*opendir)(struct vfs_handle_struct *handle, const char *fname, const char *mask, uint32 attributes);
	SMB_STRUCT_DIR *(*fdopendir)(struct vfs_handle_struct *handle, files_struct *fsp, const char *mask, uint32 attributes);
	SMB_STRUCT_DIRENT *(*readdir)(struct vfs_handle_struct *handle,
				      SMB_STRUCT_DIR *dirp,
				      SMB_STRUCT_STAT *sbuf);
	void (*seekdir)(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp, long offset);
	long (*telldir)(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp);
	void (*rewind_dir)(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp);
	int (*mkdir)(struct vfs_handle_struct *handle, const char *path, mode_t mode);
	int (*rmdir)(struct vfs_handle_struct *handle, const char *path);
	int (*closedir)(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dir);
	void (*init_search_op)(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp);

	/* File operations */

	int (*open_fn)(struct vfs_handle_struct *handle,
		       struct smb_filename *smb_fname, files_struct *fsp,
		       int flags, mode_t mode);
	NTSTATUS (*create_file)(struct vfs_handle_struct *handle,
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
				files_struct **result,
				int *pinfo);
	int (*close_fn)(struct vfs_handle_struct *handle, struct files_struct *fsp);
	ssize_t (*vfs_read)(struct vfs_handle_struct *handle, struct files_struct *fsp, void *data, size_t n);
	ssize_t (*pread)(struct vfs_handle_struct *handle, struct files_struct *fsp, void *data, size_t n, SMB_OFF_T offset);
	ssize_t (*write)(struct vfs_handle_struct *handle, struct files_struct *fsp, const void *data, size_t n);
	ssize_t (*pwrite)(struct vfs_handle_struct *handle, struct files_struct *fsp, const void *data, size_t n, SMB_OFF_T offset);
	SMB_OFF_T (*lseek)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_OFF_T offset, int whence);
	ssize_t (*sendfile)(struct vfs_handle_struct *handle, int tofd, files_struct *fromfsp, const DATA_BLOB *header, SMB_OFF_T offset, size_t count);
	ssize_t (*recvfile)(struct vfs_handle_struct *handle, int fromfd, files_struct *tofsp, SMB_OFF_T offset, size_t count);
	int (*rename)(struct vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname_src,
		      const struct smb_filename *smb_fname_dst);
	int (*fsync)(struct vfs_handle_struct *handle, struct files_struct *fsp);
	int (*stat)(struct vfs_handle_struct *handle, struct smb_filename *smb_fname);
	int (*fstat)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_STAT *sbuf);
	int (*lstat)(struct vfs_handle_struct *handle, struct smb_filename *smb_filename);
	uint64_t (*get_alloc_size)(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_STAT *sbuf);
	int (*unlink)(struct vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname);
	int (*chmod)(struct vfs_handle_struct *handle, const char *path, mode_t mode);
	int (*fchmod)(struct vfs_handle_struct *handle, struct files_struct *fsp, mode_t mode);
	int (*chown)(struct vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid);
	int (*fchown)(struct vfs_handle_struct *handle, struct files_struct *fsp, uid_t uid, gid_t gid);
	int (*lchown)(struct vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid);
	int (*chdir)(struct vfs_handle_struct *handle, const char *path);
	char *(*getwd)(struct vfs_handle_struct *handle, char *buf);
	int (*ntimes)(struct vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname,
		      struct smb_file_time *ft);
	int (*ftruncate)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_OFF_T offset);
	int (*fallocate)(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				enum vfs_fallocate_mode mode,
				SMB_OFF_T offset,
				SMB_OFF_T len);
	bool (*lock)(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_OFF_T offset, SMB_OFF_T count, int type);
	int (*kernel_flock)(struct vfs_handle_struct *handle, struct files_struct *fsp,
			    uint32 share_mode, uint32_t access_mask);
	int (*linux_setlease)(struct vfs_handle_struct *handle, struct files_struct *fsp, int leasetype);
	bool (*getlock)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid);
	int (*symlink)(struct vfs_handle_struct *handle, const char *oldpath, const char *newpath);
	int (*vfs_readlink)(struct vfs_handle_struct *handle, const char *path, char *buf, size_t bufsiz);
	int (*link)(struct vfs_handle_struct *handle, const char *oldpath, const char *newpath);
	int (*mknod)(struct vfs_handle_struct *handle, const char *path, mode_t mode, SMB_DEV_T dev);
	char *(*realpath)(struct vfs_handle_struct *handle, const char *path);
	NTSTATUS (*notify_watch)(struct vfs_handle_struct *handle,
				 struct sys_notify_context *ctx,
				 struct notify_entry *e,
				 void (*callback)(struct sys_notify_context *ctx,
						  void *private_data,
						  struct notify_event *ev),
				 void *private_data, void *handle_p);
	int (*chflags)(struct vfs_handle_struct *handle, const char *path, unsigned int flags);
	struct file_id (*file_id_create)(struct vfs_handle_struct *handle,
					 const SMB_STRUCT_STAT *sbuf);

	NTSTATUS (*streaminfo)(struct vfs_handle_struct *handle,
			       struct files_struct *fsp,
			       const char *fname,
			       TALLOC_CTX *mem_ctx,
			       unsigned int *num_streams,
			       struct stream_struct **streams);

	int (*get_real_filename)(struct vfs_handle_struct *handle,
				 const char *path,
				 const char *name,
				 TALLOC_CTX *mem_ctx,
				 char **found_name);

	const char *(*connectpath)(struct vfs_handle_struct *handle,
				   const char *filename);

	NTSTATUS (*brl_lock_windows)(struct vfs_handle_struct *handle,
				     struct byte_range_lock *br_lck,
				     struct lock_struct *plock,
				     bool blocking_lock,
				     struct blocking_lock_record *blr);

	bool (*brl_unlock_windows)(struct vfs_handle_struct *handle,
				   struct messaging_context *msg_ctx,
				   struct byte_range_lock *br_lck,
				   const struct lock_struct *plock);

	bool (*brl_cancel_windows)(struct vfs_handle_struct *handle,
				   struct byte_range_lock *br_lck,
				   struct lock_struct *plock,
				   struct blocking_lock_record *blr);

	bool (*strict_lock)(struct vfs_handle_struct *handle,
			    struct files_struct *fsp,
			    struct lock_struct *plock);

	void (*strict_unlock)(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      struct lock_struct *plock);

	NTSTATUS (*translate_name)(struct vfs_handle_struct *handle,
				   const char *name,
				   enum vfs_translate_direction direction,
				   TALLOC_CTX *mem_ctx,
				   char **mapped_name);

	/* NT ACL operations. */

	NTSTATUS (*fget_nt_acl)(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				uint32 security_info,
				struct security_descriptor **ppdesc);
	NTSTATUS (*get_nt_acl)(struct vfs_handle_struct *handle,
			       const char *name,
			       uint32 security_info,
			       struct security_descriptor **ppdesc);
	NTSTATUS (*fset_nt_acl)(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				uint32 security_info_sent,
				const struct security_descriptor *psd);

	/* POSIX ACL operations. */

	int (*chmod_acl)(struct vfs_handle_struct *handle, const char *name, mode_t mode);
	int (*fchmod_acl)(struct vfs_handle_struct *handle, struct files_struct *fsp, mode_t mode);

	int (*sys_acl_get_entry)(struct vfs_handle_struct *handle, SMB_ACL_T theacl, int entry_id, SMB_ACL_ENTRY_T *entry_p);
	int (*sys_acl_get_tag_type)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *tag_type_p);
	int (*sys_acl_get_permset)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p);
	void * (*sys_acl_get_qualifier)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry_d);
	SMB_ACL_T (*sys_acl_get_file)(struct vfs_handle_struct *handle, const char *path_p, SMB_ACL_TYPE_T type);
	SMB_ACL_T (*sys_acl_get_fd)(struct vfs_handle_struct *handle, struct files_struct *fsp);
	int (*sys_acl_clear_perms)(struct vfs_handle_struct *handle, SMB_ACL_PERMSET_T permset);
	int (*sys_acl_add_perm)(struct vfs_handle_struct *handle, SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm);
	char * (*sys_acl_to_text)(struct vfs_handle_struct *handle, SMB_ACL_T theacl, ssize_t *plen);
	SMB_ACL_T (*sys_acl_init)(struct vfs_handle_struct *handle, int count);
	int (*sys_acl_create_entry)(struct vfs_handle_struct *handle, SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry);
	int (*sys_acl_set_tag_type)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry, SMB_ACL_TAG_T tagtype);
	int (*sys_acl_set_qualifier)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry, void *qual);
	int (*sys_acl_set_permset)(struct vfs_handle_struct *handle, SMB_ACL_ENTRY_T entry, SMB_ACL_PERMSET_T permset);
	int (*sys_acl_valid)(struct vfs_handle_struct *handle, SMB_ACL_T theacl );
	int (*sys_acl_set_file)(struct vfs_handle_struct *handle, const char *name, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl);
	int (*sys_acl_set_fd)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_ACL_T theacl);
	int (*sys_acl_delete_def_file)(struct vfs_handle_struct *handle, const char *path);
	int (*sys_acl_get_perm)(struct vfs_handle_struct *handle, SMB_ACL_PERMSET_T permset, SMB_ACL_PERM_T perm);
	int (*sys_acl_free_text)(struct vfs_handle_struct *handle, char *text);
	int (*sys_acl_free_acl)(struct vfs_handle_struct *handle, SMB_ACL_T posix_acl);
	int (*sys_acl_free_qualifier)(struct vfs_handle_struct *handle, void *qualifier, SMB_ACL_TAG_T tagtype);

	/* EA operations. */
	ssize_t (*getxattr)(struct vfs_handle_struct *handle,const char *path, const char *name, void *value, size_t size);
	ssize_t (*lgetxattr)(struct vfs_handle_struct *handle,const char *path, const char *name, void *value, size_t size);
	ssize_t (*fgetxattr)(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name, void *value, size_t size);
	ssize_t (*listxattr)(struct vfs_handle_struct *handle, const char *path, char *list, size_t size);
	ssize_t (*llistxattr)(struct vfs_handle_struct *handle, const char *path, char *list, size_t size);
	ssize_t (*flistxattr)(struct vfs_handle_struct *handle, struct files_struct *fsp, char *list, size_t size);
	int (*removexattr)(struct vfs_handle_struct *handle, const char *path, const char *name);
	int (*lremovexattr)(struct vfs_handle_struct *handle, const char *path, const char *name);
	int (*fremovexattr)(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name);
	int (*setxattr)(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags);
	int (*lsetxattr)(struct vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags);
	int (*fsetxattr)(struct vfs_handle_struct *handle, struct files_struct *fsp, const char *name, const void *value, size_t size, int flags);

	/* aio operations */
	int (*aio_read)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
	int (*aio_write)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
	ssize_t (*aio_return_fn)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
	int (*aio_cancel)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
	int (*aio_error_fn)(struct vfs_handle_struct *handle, struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
	int (*aio_fsync)(struct vfs_handle_struct *handle, struct files_struct *fsp, int op, SMB_STRUCT_AIOCB *aiocb);
	int (*aio_suspend)(struct vfs_handle_struct *handle, struct files_struct *fsp, const SMB_STRUCT_AIOCB * const aiocb[], int n, const struct timespec *timeout);
	bool (*aio_force)(struct vfs_handle_struct *handle, struct files_struct *fsp);

	/* offline operations */
	bool (*is_offline)(struct vfs_handle_struct *handle,
			   const struct smb_filename *fname,
			   SMB_STRUCT_STAT *sbuf);
	int (*set_offline)(struct vfs_handle_struct *handle,
			   const struct smb_filename *fname);
};

/*
    VFS operation description. Each VFS module registers an array of vfs_op_tuple to VFS subsystem,
    which describes all operations this module is willing to intercept.
    VFS subsystem initializes then the conn->vfs_ops and conn->vfs_opaque_ops structs
    using this information.
*/

typedef struct vfs_handle_struct {
	struct vfs_handle_struct  *next, *prev;
	const char *param;
	struct connection_struct *conn;
	const struct vfs_fn_pointers *fns;
	void *data;
	void (*free_data)(void **data);
} vfs_handle_struct;


typedef struct vfs_statvfs_struct {
	/* For undefined recommended transfer size return -1 in that field */
	uint32 OptimalTransferSize;  /* bsize on some os, iosize on other os */
	uint32 BlockSize;

	/*
	 The next three fields are in terms of the block size.
	 (above). If block size is unknown, 4096 would be a
	 reasonable block size for a server to report.
	 Note that returning the blocks/blocksavail removes need
	 to make a second call (to QFSInfo level 0x103 to get this info.
	 UserBlockAvail is typically less than or equal to BlocksAvail,
	 if no distinction is made return the same value in each.
	*/

	uint64_t TotalBlocks;
	uint64_t BlocksAvail;       /* bfree */
	uint64_t UserBlocksAvail;   /* bavail */

	/* For undefined Node fields or FSID return -1 */
	uint64_t TotalFileNodes;
	uint64_t FreeFileNodes;
	uint64_t FsIdentifier;   /* fsid */
	/* NB Namelen comes from FILE_SYSTEM_ATTRIBUTE_INFO call */
	/* NB flags can come from FILE_SYSTEM_DEVICE_INFO call   */

	int FsCapabilities;
} vfs_statvfs_struct;

/* Add a new FSP extension of the given type. Returns a pointer to the
 * extenstion data.
 */
#define VFS_ADD_FSP_EXTENSION(handle, fsp, type, destroy_fn)		\
    vfs_add_fsp_extension_notype(handle, (fsp), sizeof(type), (destroy_fn))

/* Return a pointer to the existing FSP extension data. */
#define VFS_FETCH_FSP_EXTENSION(handle, fsp) \
    vfs_fetch_fsp_extension(handle, (fsp))

/* Return the talloc context associated with an FSP extension. */
#define VFS_MEMCTX_FSP_EXTENSION(handle, fsp) \
    vfs_memctx_fsp_extension(handle, (fsp))

/* Remove and destroy an FSP extension. */
#define VFS_REMOVE_FSP_EXTENSION(handle, fsp) \
    vfs_remove_fsp_extension((handle), (fsp))

#define SMB_VFS_HANDLE_GET_DATA(handle, datap, type, ret) { \
	if (!(handle)||((datap=(type *)(handle)->data)==NULL)) { \
		DEBUG(0,("%s() failed to get vfs_handle->data!\n",__FUNCTION__)); \
		ret; \
	} \
}

#define SMB_VFS_HANDLE_SET_DATA(handle, datap, free_fn, type, ret) { \
	if (!(handle)) { \
		DEBUG(0,("%s() failed to set handle->data!\n",__FUNCTION__)); \
		ret; \
	} else { \
		if ((handle)->free_data) { \
			(handle)->free_data(&(handle)->data); \
		} \
		(handle)->data = (void *)datap; \
		(handle)->free_data = free_fn; \
	} \
}

#define SMB_VFS_HANDLE_FREE_DATA(handle) { \
	if ((handle) && (handle)->free_data) { \
		(handle)->free_data(&(handle)->data); \
	} \
}

/* Check whether module-specific data handle was already allocated or not */
#define SMB_VFS_HANDLE_TEST_DATA(handle)  ( !(handle) || !(handle)->data ? False : True )

#define SMB_VFS_OP(x) ((void *) x)

#define DEFAULT_VFS_MODULE_NAME "/[Default VFS]/"

#include "vfs_macros.h"

int smb_vfs_call_connect(struct vfs_handle_struct *handle,
			 const char *service, const char *user);
void smb_vfs_call_disconnect(struct vfs_handle_struct *handle);
uint64_t smb_vfs_call_disk_free(struct vfs_handle_struct *handle,
				const char *path, bool small_query,
				uint64_t *bsize, uint64_t *dfree,
				uint64_t *dsize);
int smb_vfs_call_get_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt);
int smb_vfs_call_set_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt);
int smb_vfs_call_get_shadow_copy_data(struct vfs_handle_struct *handle,
				      struct files_struct *fsp,
				      struct shadow_copy_data *shadow_copy_data,
				      bool labels);
int smb_vfs_call_statvfs(struct vfs_handle_struct *handle, const char *path,
			 struct vfs_statvfs_struct *statbuf);
uint32_t smb_vfs_call_fs_capabilities(struct vfs_handle_struct *handle,
			enum timestamp_set_resolution *p_ts_res);
SMB_STRUCT_DIR *smb_vfs_call_opendir(struct vfs_handle_struct *handle,
				     const char *fname, const char *mask,
				     uint32 attributes);
SMB_STRUCT_DIR *smb_vfs_call_fdopendir(struct vfs_handle_struct *handle,
					struct files_struct *fsp,
					const char *mask,
					uint32 attributes);
SMB_STRUCT_DIRENT *smb_vfs_call_readdir(struct vfs_handle_struct *handle,
					SMB_STRUCT_DIR *dirp,
					SMB_STRUCT_STAT *sbuf);
void smb_vfs_call_seekdir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp, long offset);
long smb_vfs_call_telldir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp);
void smb_vfs_call_rewind_dir(struct vfs_handle_struct *handle,
			     SMB_STRUCT_DIR *dirp);
int smb_vfs_call_mkdir(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode);
int smb_vfs_call_rmdir(struct vfs_handle_struct *handle, const char *path);
int smb_vfs_call_closedir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dir);
void smb_vfs_call_init_search_op(struct vfs_handle_struct *handle,
				 SMB_STRUCT_DIR *dirp);
int smb_vfs_call_open(struct vfs_handle_struct *handle,
		      struct smb_filename *smb_fname, struct files_struct *fsp,
		      int flags, mode_t mode);
NTSTATUS smb_vfs_call_create_file(struct vfs_handle_struct *handle,
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
				  files_struct **result,
				  int *pinfo);
int smb_vfs_call_close_fn(struct vfs_handle_struct *handle,
			  struct files_struct *fsp);
ssize_t smb_vfs_call_vfs_read(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, void *data, size_t n);
ssize_t smb_vfs_call_pread(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, void *data, size_t n,
			   SMB_OFF_T offset);
ssize_t smb_vfs_call_write(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, const void *data,
			   size_t n);
ssize_t smb_vfs_call_pwrite(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, const void *data,
			    size_t n, SMB_OFF_T offset);
SMB_OFF_T smb_vfs_call_lseek(struct vfs_handle_struct *handle,
			     struct files_struct *fsp, SMB_OFF_T offset,
			     int whence);
ssize_t smb_vfs_call_sendfile(struct vfs_handle_struct *handle, int tofd,
			      files_struct *fromfsp, const DATA_BLOB *header,
			      SMB_OFF_T offset, size_t count);
ssize_t smb_vfs_call_recvfile(struct vfs_handle_struct *handle, int fromfd,
			      files_struct *tofsp, SMB_OFF_T offset,
			      size_t count);
int smb_vfs_call_rename(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname_src,
			const struct smb_filename *smb_fname_dst);
int smb_vfs_call_fsync(struct vfs_handle_struct *handle,
		       struct files_struct *fsp);
int smb_vfs_call_stat(struct vfs_handle_struct *handle,
		      struct smb_filename *smb_fname);
int smb_vfs_call_fstat(struct vfs_handle_struct *handle,
		       struct files_struct *fsp, SMB_STRUCT_STAT *sbuf);
int smb_vfs_call_lstat(struct vfs_handle_struct *handle,
		       struct smb_filename *smb_filename);
uint64_t smb_vfs_call_get_alloc_size(struct vfs_handle_struct *handle,
				     struct files_struct *fsp,
				     const SMB_STRUCT_STAT *sbuf);
int smb_vfs_call_unlink(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname);
int smb_vfs_call_chmod(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode);
int smb_vfs_call_fchmod(struct vfs_handle_struct *handle,
			struct files_struct *fsp, mode_t mode);
int smb_vfs_call_chown(struct vfs_handle_struct *handle, const char *path,
		       uid_t uid, gid_t gid);
int smb_vfs_call_fchown(struct vfs_handle_struct *handle,
			struct files_struct *fsp, uid_t uid, gid_t gid);
int smb_vfs_call_lchown(struct vfs_handle_struct *handle, const char *path,
			uid_t uid, gid_t gid);
int smb_vfs_call_chdir(struct vfs_handle_struct *handle, const char *path);
char *smb_vfs_call_getwd(struct vfs_handle_struct *handle, char *buf);
int smb_vfs_call_ntimes(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname,
			struct smb_file_time *ft);
int smb_vfs_call_ftruncate(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, SMB_OFF_T offset);
int smb_vfs_call_fallocate(struct vfs_handle_struct *handle,
			struct files_struct *fsp,
			enum vfs_fallocate_mode mode,
			SMB_OFF_T offset,
			SMB_OFF_T len);
bool smb_vfs_call_lock(struct vfs_handle_struct *handle,
		       struct files_struct *fsp, int op, SMB_OFF_T offset,
		       SMB_OFF_T count, int type);
int smb_vfs_call_kernel_flock(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, uint32 share_mode,
			      uint32_t access_mask);
int smb_vfs_call_linux_setlease(struct vfs_handle_struct *handle,
				struct files_struct *fsp, int leasetype);
bool smb_vfs_call_getlock(struct vfs_handle_struct *handle,
			  struct files_struct *fsp, SMB_OFF_T *poffset,
			  SMB_OFF_T *pcount, int *ptype, pid_t *ppid);
int smb_vfs_call_symlink(struct vfs_handle_struct *handle, const char *oldpath,
			 const char *newpath);
int smb_vfs_call_vfs_readlink(struct vfs_handle_struct *handle,
			      const char *path, char *buf, size_t bufsiz);
int smb_vfs_call_link(struct vfs_handle_struct *handle, const char *oldpath,
		      const char *newpath);
int smb_vfs_call_mknod(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode, SMB_DEV_T dev);
char *smb_vfs_call_realpath(struct vfs_handle_struct *handle, const char *path);
NTSTATUS smb_vfs_call_notify_watch(struct vfs_handle_struct *handle,
				   struct sys_notify_context *ctx,
				   struct notify_entry *e,
				   void (*callback)(struct sys_notify_context *ctx,
						    void *private_data,
						    struct notify_event *ev),
				   void *private_data, void *handle_p);
int smb_vfs_call_chflags(struct vfs_handle_struct *handle, const char *path,
			 unsigned int flags);
struct file_id smb_vfs_call_file_id_create(struct vfs_handle_struct *handle,
					   const SMB_STRUCT_STAT *sbuf);
NTSTATUS smb_vfs_call_streaminfo(struct vfs_handle_struct *handle,
				 struct files_struct *fsp,
				 const char *fname,
				 TALLOC_CTX *mem_ctx,
				 unsigned int *num_streams,
				 struct stream_struct **streams);
int smb_vfs_call_get_real_filename(struct vfs_handle_struct *handle,
				   const char *path, const char *name,
				   TALLOC_CTX *mem_ctx, char **found_name);
const char *smb_vfs_call_connectpath(struct vfs_handle_struct *handle,
				     const char *filename);
NTSTATUS smb_vfs_call_brl_lock_windows(struct vfs_handle_struct *handle,
				       struct byte_range_lock *br_lck,
				       struct lock_struct *plock,
				       bool blocking_lock,
				       struct blocking_lock_record *blr);
bool smb_vfs_call_brl_unlock_windows(struct vfs_handle_struct *handle,
				     struct messaging_context *msg_ctx,
				     struct byte_range_lock *br_lck,
				     const struct lock_struct *plock);
bool smb_vfs_call_brl_cancel_windows(struct vfs_handle_struct *handle,
				     struct byte_range_lock *br_lck,
				     struct lock_struct *plock,
				     struct blocking_lock_record *blr);
bool smb_vfs_call_strict_lock(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      struct lock_struct *plock);
void smb_vfs_call_strict_unlock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock);
NTSTATUS smb_vfs_call_translate_name(struct vfs_handle_struct *handle,
				     const char *name,
				     enum vfs_translate_direction direction,
				     TALLOC_CTX *mem_ctx,
				     char **mapped_name);
NTSTATUS smb_vfs_call_fget_nt_acl(struct vfs_handle_struct *handle,
				  struct files_struct *fsp,
				  uint32 security_info,
				  struct security_descriptor **ppdesc);
NTSTATUS smb_vfs_call_get_nt_acl(struct vfs_handle_struct *handle,
				 const char *name,
				 uint32 security_info,
				 struct security_descriptor **ppdesc);
NTSTATUS smb_vfs_call_fset_nt_acl(struct vfs_handle_struct *handle,
				  struct files_struct *fsp,
				  uint32 security_info_sent,
				  const struct security_descriptor *psd);
int smb_vfs_call_chmod_acl(struct vfs_handle_struct *handle, const char *name,
			   mode_t mode);
int smb_vfs_call_fchmod_acl(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, mode_t mode);
int smb_vfs_call_sys_acl_get_entry(struct vfs_handle_struct *handle,
				   SMB_ACL_T theacl, int entry_id,
				   SMB_ACL_ENTRY_T *entry_p);
int smb_vfs_call_sys_acl_get_tag_type(struct vfs_handle_struct *handle,
				      SMB_ACL_ENTRY_T entry_d,
				      SMB_ACL_TAG_T *tag_type_p);
int smb_vfs_call_sys_acl_get_permset(struct vfs_handle_struct *handle,
				     SMB_ACL_ENTRY_T entry_d,
				     SMB_ACL_PERMSET_T *permset_p);
void * smb_vfs_call_sys_acl_get_qualifier(struct vfs_handle_struct *handle,
					  SMB_ACL_ENTRY_T entry_d);
SMB_ACL_T smb_vfs_call_sys_acl_get_file(struct vfs_handle_struct *handle,
					const char *path_p,
					SMB_ACL_TYPE_T type);
SMB_ACL_T smb_vfs_call_sys_acl_get_fd(struct vfs_handle_struct *handle,
				      struct files_struct *fsp);
int smb_vfs_call_sys_acl_clear_perms(struct vfs_handle_struct *handle,
				     SMB_ACL_PERMSET_T permset);
int smb_vfs_call_sys_acl_add_perm(struct vfs_handle_struct *handle,
				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm);
char * smb_vfs_call_sys_acl_to_text(struct vfs_handle_struct *handle,
				    SMB_ACL_T theacl, ssize_t *plen);
SMB_ACL_T smb_vfs_call_sys_acl_init(struct vfs_handle_struct *handle,
				    int count);
int smb_vfs_call_sys_acl_create_entry(struct vfs_handle_struct *handle,
				      SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry);
int smb_vfs_call_sys_acl_set_tag_type(struct vfs_handle_struct *handle,
				      SMB_ACL_ENTRY_T entry,
				      SMB_ACL_TAG_T tagtype);
int smb_vfs_call_sys_acl_set_qualifier(struct vfs_handle_struct *handle,
				       SMB_ACL_ENTRY_T entry, void *qual);
int smb_vfs_call_sys_acl_set_permset(struct vfs_handle_struct *handle,
				     SMB_ACL_ENTRY_T entry,
				     SMB_ACL_PERMSET_T permset);
int smb_vfs_call_sys_acl_valid(struct vfs_handle_struct *handle,
			       SMB_ACL_T theacl);
int smb_vfs_call_sys_acl_set_file(struct vfs_handle_struct *handle,
				  const char *name, SMB_ACL_TYPE_T acltype,
				  SMB_ACL_T theacl);
int smb_vfs_call_sys_acl_set_fd(struct vfs_handle_struct *handle,
				struct files_struct *fsp, SMB_ACL_T theacl);
int smb_vfs_call_sys_acl_delete_def_file(struct vfs_handle_struct *handle,
					 const char *path);
int smb_vfs_call_sys_acl_get_perm(struct vfs_handle_struct *handle,
				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm);
int smb_vfs_call_sys_acl_free_text(struct vfs_handle_struct *handle,
				   char *text);
int smb_vfs_call_sys_acl_free_acl(struct vfs_handle_struct *handle,
				  SMB_ACL_T posix_acl);
int smb_vfs_call_sys_acl_free_qualifier(struct vfs_handle_struct *handle,
					void *qualifier, SMB_ACL_TAG_T tagtype);
ssize_t smb_vfs_call_getxattr(struct vfs_handle_struct *handle,
			      const char *path, const char *name, void *value,
			      size_t size);
ssize_t smb_vfs_call_lgetxattr(struct vfs_handle_struct *handle,
			       const char *path, const char *name, void *value,
			       size_t size);
ssize_t smb_vfs_call_fgetxattr(struct vfs_handle_struct *handle,
			       struct files_struct *fsp, const char *name,
			       void *value, size_t size);
ssize_t smb_vfs_call_listxattr(struct vfs_handle_struct *handle,
			       const char *path, char *list, size_t size);
ssize_t smb_vfs_call_llistxattr(struct vfs_handle_struct *handle,
				const char *path, char *list, size_t size);
ssize_t smb_vfs_call_flistxattr(struct vfs_handle_struct *handle,
				struct files_struct *fsp, char *list,
				size_t size);
int smb_vfs_call_removexattr(struct vfs_handle_struct *handle,
			     const char *path, const char *name);
int smb_vfs_call_lremovexattr(struct vfs_handle_struct *handle,
			      const char *path, const char *name);
int smb_vfs_call_fremovexattr(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, const char *name);
int smb_vfs_call_setxattr(struct vfs_handle_struct *handle, const char *path,
			  const char *name, const void *value, size_t size,
			  int flags);
int smb_vfs_call_lsetxattr(struct vfs_handle_struct *handle, const char *path,
			   const char *name, const void *value, size_t size,
			   int flags);
int smb_vfs_call_fsetxattr(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, const char *name,
			   const void *value, size_t size, int flags);
int smb_vfs_call_aio_read(struct vfs_handle_struct *handle,
			  struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
int smb_vfs_call_aio_write(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
ssize_t smb_vfs_call_aio_return_fn(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   SMB_STRUCT_AIOCB *aiocb);
int smb_vfs_call_aio_cancel(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb);
int smb_vfs_call_aio_error_fn(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      SMB_STRUCT_AIOCB *aiocb);
int smb_vfs_call_aio_fsync(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, int op,
			   SMB_STRUCT_AIOCB *aiocb);
int smb_vfs_call_aio_suspend(struct vfs_handle_struct *handle,
			     struct files_struct *fsp,
			     const SMB_STRUCT_AIOCB * const aiocb[], int n,
			     const struct timespec *timeout);
bool smb_vfs_call_aio_force(struct vfs_handle_struct *handle,
			    struct files_struct *fsp);
bool smb_vfs_call_is_offline(struct vfs_handle_struct *handle,
			     const struct smb_filename *fname,
			     SMB_STRUCT_STAT *sbuf);
int smb_vfs_call_set_offline(struct vfs_handle_struct *handle,
			     const struct smb_filename *fname);

#endif /* _VFS_H */
