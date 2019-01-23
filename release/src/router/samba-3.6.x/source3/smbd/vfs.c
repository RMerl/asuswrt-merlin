/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   VFS initialisation and support functions
   Copyright (C) Tim Potter 1999
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) James Peach 2006
   Copyright (C) Volker Lendecke 2009

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

#include "includes.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "memcache.h"
#include "transfer_file.h"
#include "ntioctl.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

static_decl_vfs;

struct vfs_init_function_entry {
	char *name;
	struct vfs_init_function_entry *prev, *next;
	const struct vfs_fn_pointers *fns;
};

/****************************************************************************
    maintain the list of available backends
****************************************************************************/

static struct vfs_init_function_entry *vfs_find_backend_entry(const char *name)
{
	struct vfs_init_function_entry *entry = backends;

	DEBUG(10, ("vfs_find_backend_entry called for %s\n", name));

	while(entry) {
		if (strcmp(entry->name, name)==0) return entry;
		entry = entry->next;
	}

	return NULL;
}

NTSTATUS smb_register_vfs(int version, const char *name,
			  const struct vfs_fn_pointers *fns)
{
	struct vfs_init_function_entry *entry = backends;

 	if ((version != SMB_VFS_INTERFACE_VERSION)) {
		DEBUG(0, ("Failed to register vfs module.\n"
		          "The module was compiled against SMB_VFS_INTERFACE_VERSION %d,\n"
		          "current SMB_VFS_INTERFACE_VERSION is %d.\n"
		          "Please recompile against the current Samba Version!\n",  
			  version, SMB_VFS_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
  	}

	if (!name || !name[0]) {
		DEBUG(0,("smb_register_vfs() called with NULL pointer or empty name!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (vfs_find_backend_entry(name)) {
		DEBUG(0,("VFS module %s already loaded!\n", name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	entry = SMB_XMALLOC_P(struct vfs_init_function_entry);
	entry->name = smb_xstrdup(name);
	entry->fns = fns;

	DLIST_ADD(backends, entry);
	DEBUG(5, ("Successfully added vfs backend '%s'\n", name));
	return NT_STATUS_OK;
}

/****************************************************************************
  initialise default vfs hooks
****************************************************************************/

static void vfs_init_default(connection_struct *conn)
{
	DEBUG(3, ("Initialising default vfs hooks\n"));
	vfs_init_custom(conn, DEFAULT_VFS_MODULE_NAME);
}

/****************************************************************************
  initialise custom vfs hooks
 ****************************************************************************/

bool vfs_init_custom(connection_struct *conn, const char *vfs_object)
{
	char *module_path = NULL;
	char *module_name = NULL;
	char *module_param = NULL, *p;
	vfs_handle_struct *handle;
	const struct vfs_init_function_entry *entry;

	if (!conn||!vfs_object||!vfs_object[0]) {
		DEBUG(0, ("vfs_init_custom() called with NULL pointer or "
			  "empty vfs_object!\n"));
		return False;
	}

	if(!backends) {
		static_init_vfs;
	}

	DEBUG(3, ("Initialising custom vfs hooks from [%s]\n", vfs_object));

	module_path = smb_xstrdup(vfs_object);

	p = strchr_m(module_path, ':');

	if (p) {
		*p = 0;
		module_param = p+1;
		trim_char(module_param, ' ', ' ');
	}

	trim_char(module_path, ' ', ' ');

	module_name = smb_xstrdup(module_path);

	if ((module_name[0] == '/') &&
	    (strcmp(module_path, DEFAULT_VFS_MODULE_NAME) != 0)) {

		/*
		 * Extract the module name from the path. Just use the base
		 * name of the last path component.
		 */

		SAFE_FREE(module_name);
		module_name = smb_xstrdup(strrchr_m(module_path, '/')+1);

		p = strchr_m(module_name, '.');

		if (p != NULL) {
			*p = '\0';
		}
	}

	/* First, try to load the module with the new module system */
	entry = vfs_find_backend_entry(module_name);
	if (!entry) {
		NTSTATUS status;

		DEBUG(5, ("vfs module [%s] not loaded - trying to load...\n",
			  vfs_object));

		status = smb_probe_module("vfs", module_path);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("error probing vfs module '%s': %s\n",
				  module_path, nt_errstr(status)));
			goto fail;
		}

		entry = vfs_find_backend_entry(module_name);
		if (!entry) {
			DEBUG(0,("Can't find a vfs module [%s]\n",vfs_object));
			goto fail;
		}
	}

	DEBUGADD(5,("Successfully loaded vfs module [%s] with the new modules system\n", vfs_object));

	handle = TALLOC_ZERO_P(conn, vfs_handle_struct);
	if (!handle) {
		DEBUG(0,("TALLOC_ZERO() failed!\n"));
		goto fail;
	}
	handle->conn = conn;
	handle->fns = entry->fns;
	if (module_param) {
		handle->param = talloc_strdup(conn, module_param);
	}
	DLIST_ADD(conn->vfs_handles, handle);

	SAFE_FREE(module_path);
	SAFE_FREE(module_name);
	return True;

 fail:
	SAFE_FREE(module_path);
	SAFE_FREE(module_name);
	return False;
}

/*****************************************************************
 Allow VFS modules to extend files_struct with VFS-specific state.
 This will be ok for small numbers of extensions, but might need to
 be refactored if it becomes more widely used.
******************************************************************/

#define EXT_DATA_AREA(e) ((uint8 *)(e) + sizeof(struct vfs_fsp_data))

void *vfs_add_fsp_extension_notype(vfs_handle_struct *handle,
				   files_struct *fsp, size_t ext_size,
				   void (*destroy_fn)(void *p_data))
{
	struct vfs_fsp_data *ext;
	void * ext_data;

	/* Prevent VFS modules adding multiple extensions. */
	if ((ext_data = vfs_fetch_fsp_extension(handle, fsp))) {
		return ext_data;
	}

	ext = (struct vfs_fsp_data *)TALLOC_ZERO(
		handle->conn, sizeof(struct vfs_fsp_data) + ext_size);
	if (ext == NULL) {
		return NULL;
	}

	ext->owner = handle;
	ext->next = fsp->vfs_extension;
	ext->destroy = destroy_fn;
	fsp->vfs_extension = ext;
	return EXT_DATA_AREA(ext);
}

void vfs_remove_fsp_extension(vfs_handle_struct *handle, files_struct *fsp)
{
	struct vfs_fsp_data *curr;
	struct vfs_fsp_data *prev;

	for (curr = fsp->vfs_extension, prev = NULL;
	     curr;
	     prev = curr, curr = curr->next) {
		if (curr->owner == handle) {
		    if (prev) {
			    prev->next = curr->next;
		    } else {
			    fsp->vfs_extension = curr->next;
		    }
		    if (curr->destroy) {
			    curr->destroy(EXT_DATA_AREA(curr));
		    }
		    TALLOC_FREE(curr);
		    return;
		}
	}
}

void *vfs_memctx_fsp_extension(vfs_handle_struct *handle, files_struct *fsp)
{
	struct vfs_fsp_data *head;

	for (head = fsp->vfs_extension; head; head = head->next) {
		if (head->owner == handle) {
			return head;
		}
	}

	return NULL;
}

void *vfs_fetch_fsp_extension(vfs_handle_struct *handle, files_struct *fsp)
{
	struct vfs_fsp_data *head;

	head = (struct vfs_fsp_data *)vfs_memctx_fsp_extension(handle, fsp);
	if (head != NULL) {
		return EXT_DATA_AREA(head);
	}

	return NULL;
}

#undef EXT_DATA_AREA

/*****************************************************************
 Generic VFS init.
******************************************************************/

bool smbd_vfs_init(connection_struct *conn)
{
	const char **vfs_objects;
	unsigned int i = 0;
	int j = 0;

	/* Normal share - initialise with disk access functions */
	vfs_init_default(conn);
	vfs_objects = lp_vfs_objects(SNUM(conn));

	/* Override VFS functions if 'vfs object' was not specified*/
	if (!vfs_objects || !vfs_objects[0])
		return True;

	for (i=0; vfs_objects[i] ;) {
		i++;
	}

	for (j=i-1; j >= 0; j--) {
		if (!vfs_init_custom(conn, vfs_objects[j])) {
			DEBUG(0, ("smbd_vfs_init: vfs_init_custom failed for %s\n", vfs_objects[j]));
			return False;
		}
	}
	return True;
}

/*******************************************************************
 Check if a file exists in the vfs.
********************************************************************/

NTSTATUS vfs_file_exist(connection_struct *conn, struct smb_filename *smb_fname)
{
	/* Only return OK if stat was successful and S_ISREG */
	if ((SMB_VFS_STAT(conn, smb_fname) != -1) &&
	    S_ISREG(smb_fname->st.st_ex_mode)) {
		return NT_STATUS_OK;
	}

	return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/****************************************************************************
 Read data from fsp on the vfs. (note: EINTR re-read differs from vfs_write_data)
****************************************************************************/

ssize_t vfs_read_data(files_struct *fsp, char *buf, size_t byte_count)
{
	size_t total=0;

	while (total < byte_count)
	{
		ssize_t ret = SMB_VFS_READ(fsp, buf + total,
					   byte_count - total);

		if (ret == 0) return total;
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		}
		total += ret;
	}
	return (ssize_t)total;
}

ssize_t vfs_pread_data(files_struct *fsp, char *buf,
                size_t byte_count, SMB_OFF_T offset)
{
	size_t total=0;

	while (total < byte_count)
	{
		ssize_t ret = SMB_VFS_PREAD(fsp, buf + total,
					byte_count - total, offset + total);

		if (ret == 0) return total;
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		}
		total += ret;
	}
	return (ssize_t)total;
}

/****************************************************************************
 Write data to a fd on the vfs.
****************************************************************************/

ssize_t vfs_write_data(struct smb_request *req,
			files_struct *fsp,
			const char *buffer,
			size_t N)
{
	size_t total=0;
	ssize_t ret;

	if (req && req->unread_bytes) {
		SMB_ASSERT(req->unread_bytes == N);
		/* VFS_RECVFILE must drain the socket
		 * before returning. */
		req->unread_bytes = 0;
		return SMB_VFS_RECVFILE(req->sconn->sock,
					fsp,
					(SMB_OFF_T)-1,
					N);
	}

	while (total < N) {
		ret = SMB_VFS_WRITE(fsp, buffer + total, N - total);

		if (ret == -1)
			return -1;
		if (ret == 0)
			return total;

		total += ret;
	}
	return (ssize_t)total;
}

ssize_t vfs_pwrite_data(struct smb_request *req,
			files_struct *fsp,
			const char *buffer,
			size_t N,
			SMB_OFF_T offset)
{
	size_t total=0;
	ssize_t ret;

	if (req && req->unread_bytes) {
		SMB_ASSERT(req->unread_bytes == N);
		/* VFS_RECVFILE must drain the socket
		 * before returning. */
		req->unread_bytes = 0;
		return SMB_VFS_RECVFILE(req->sconn->sock,
					fsp,
					offset,
					N);
	}

	while (total < N) {
		ret = SMB_VFS_PWRITE(fsp, buffer + total, N - total,
				     offset + total);

		if (ret == -1)
			return -1;
		if (ret == 0)
			return total;

		total += ret;
	}
	return (ssize_t)total;
}
/****************************************************************************
 An allocate file space call using the vfs interface.
 Allocates space for a file from a filedescriptor.
 Returns 0 on success, -1 on failure.
****************************************************************************/

int vfs_allocate_file_space(files_struct *fsp, uint64_t len)
{
	int ret;
	connection_struct *conn = fsp->conn;
	uint64_t space_avail;
	uint64_t bsize,dfree,dsize;
	NTSTATUS status;

	/*
	 * Actually try and commit the space on disk....
	 */

	DEBUG(10,("vfs_allocate_file_space: file %s, len %.0f\n",
		  fsp_str_dbg(fsp), (double)len));

	if (((SMB_OFF_T)len) < 0) {
		DEBUG(0,("vfs_allocate_file_space: %s negative len "
			 "requested.\n", fsp_str_dbg(fsp)));
		errno = EINVAL;
		return -1;
	}

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	if (len == (uint64_t)fsp->fsp_name->st.st_ex_size)
		return 0;

	if (len < (uint64_t)fsp->fsp_name->st.st_ex_size) {
		/* Shrink - use ftruncate. */

		DEBUG(10,("vfs_allocate_file_space: file %s, shrink. Current "
			  "size %.0f\n", fsp_str_dbg(fsp),
			  (double)fsp->fsp_name->st.st_ex_size));

		contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_ALLOC_SHRINK);

		flush_write_cache(fsp, SIZECHANGE_FLUSH);
		if ((ret = SMB_VFS_FTRUNCATE(fsp, (SMB_OFF_T)len)) != -1) {
			set_filelen_write_cache(fsp, len);
		}

		contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_ALLOC_SHRINK);

		return ret;
	}

	if (!lp_strict_allocate(SNUM(fsp->conn)))
		return 0;

	/* Grow - we need to test if we have enough space. */

	contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_ALLOC_GROW);

	/* See if we have a syscall that will allocate beyond end-of-file
	   without changing EOF. */
	ret = SMB_VFS_FALLOCATE(fsp, VFS_FALLOCATE_KEEP_SIZE, 0, len);

	contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_ALLOC_GROW);

	if (ret == 0) {
		/* We changed the allocation size on disk, but not
		   EOF - exactly as required. We're done ! */
		return 0;
	}

	len -= fsp->fsp_name->st.st_ex_size;
	len /= 1024; /* Len is now number of 1k blocks needed. */
	space_avail = get_dfree_info(conn, fsp->fsp_name->base_name, false,
				     &bsize, &dfree, &dsize);
	if (space_avail == (uint64_t)-1) {
		return -1;
	}

	DEBUG(10,("vfs_allocate_file_space: file %s, grow. Current size %.0f, "
		  "needed blocks = %.0f, space avail = %.0f\n",
		  fsp_str_dbg(fsp), (double)fsp->fsp_name->st.st_ex_size, (double)len,
		  (double)space_avail));

	if (len > space_avail) {
		errno = ENOSPC;
		return -1;
	}

	return 0;
}

/****************************************************************************
 A vfs set_filelen call.
 set the length of a file from a filedescriptor.
 Returns 0 on success, -1 on failure.
****************************************************************************/

int vfs_set_filelen(files_struct *fsp, SMB_OFF_T len)
{
	int ret;

	contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_SET_FILE_LEN);

	DEBUG(10,("vfs_set_filelen: ftruncate %s to len %.0f\n",
		  fsp_str_dbg(fsp), (double)len));
	flush_write_cache(fsp, SIZECHANGE_FLUSH);
	if ((ret = SMB_VFS_FTRUNCATE(fsp, len)) != -1) {
		set_filelen_write_cache(fsp, len);
		notify_fname(fsp->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_SIZE
			     | FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_SET_FILE_LEN);

	return ret;
}

/****************************************************************************
 A slow version of fallocate. Fallback code if SMB_VFS_FALLOCATE
 fails. Needs to be outside of the default version of SMB_VFS_FALLOCATE
 as this is also called from the default SMB_VFS_FTRUNCATE code.
 Always extends the file size.
 Returns 0 on success, errno on failure.
****************************************************************************/

#define SPARSE_BUF_WRITE_SIZE (32*1024)

int vfs_slow_fallocate(files_struct *fsp, SMB_OFF_T offset, SMB_OFF_T len)
{
	ssize_t pwrite_ret;
	size_t total = 0;

	if (!sparse_buf) {
		sparse_buf = SMB_CALLOC_ARRAY(char, SPARSE_BUF_WRITE_SIZE);
		if (!sparse_buf) {
			errno = ENOMEM;
			return ENOMEM;
		}
	}

	while (total < len) {
		size_t curr_write_size = MIN(SPARSE_BUF_WRITE_SIZE, (len - total));

		pwrite_ret = SMB_VFS_PWRITE(fsp, sparse_buf, curr_write_size, offset + total);
		if (pwrite_ret == -1) {
			DEBUG(10,("vfs_slow_fallocate: SMB_VFS_PWRITE for file "
				  "%s failed with error %s\n",
				  fsp_str_dbg(fsp), strerror(errno)));
			return errno;
		}
		total += pwrite_ret;
	}

	return 0;
}

/****************************************************************************
 A vfs fill sparse call.
 Writes zeros from the end of file to len, if len is greater than EOF.
 Used only by strict_sync.
 Returns 0 on success, -1 on failure.
****************************************************************************/

int vfs_fill_sparse(files_struct *fsp, SMB_OFF_T len)
{
	int ret;
	NTSTATUS status;
	SMB_OFF_T offset;
	size_t num_to_write;

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	if (len <= fsp->fsp_name->st.st_ex_size) {
		return 0;
	}

#ifdef S_ISFIFO
	if (S_ISFIFO(fsp->fsp_name->st.st_ex_mode)) {
		return 0;
	}
#endif

	DEBUG(10,("vfs_fill_sparse: write zeros in file %s from len %.0f to "
		  "len %.0f (%.0f bytes)\n", fsp_str_dbg(fsp),
		  (double)fsp->fsp_name->st.st_ex_size, (double)len,
		  (double)(len - fsp->fsp_name->st.st_ex_size)));

	contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_FILL_SPARSE);

	flush_write_cache(fsp, SIZECHANGE_FLUSH);

	offset = fsp->fsp_name->st.st_ex_size;
	num_to_write = len - fsp->fsp_name->st.st_ex_size;

	/* Only do this on non-stream file handles. */
	if (fsp->base_fsp == NULL) {
		/* for allocation try fallocate first. This can fail on some
		 * platforms e.g. when the filesystem doesn't support it and no
		 * emulation is being done by the libc (like on AIX with JFS1). In that
		 * case we do our own emulation. fallocate implementations can
		 * return ENOTSUP or EINVAL in cases like that. */
		ret = SMB_VFS_FALLOCATE(fsp, VFS_FALLOCATE_EXTEND_SIZE,
				offset, num_to_write);
		if (ret == ENOSPC) {
			errno = ENOSPC;
			ret = -1;
			goto out;
		}
		if (ret == 0) {
			goto out;
		}
		DEBUG(10,("vfs_fill_sparse: SMB_VFS_FALLOCATE failed with "
			"error %d. Falling back to slow manual allocation\n", ret));
	}

	ret = vfs_slow_fallocate(fsp, offset, num_to_write);
	if (ret != 0) {
		errno = ret;
		ret = -1;
	}

 out:

	if (ret == 0) {
		set_filelen_write_cache(fsp, len);
	}

	contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_FILL_SPARSE);
	return ret;
}

/****************************************************************************
 Transfer some data (n bytes) between two file_struct's.
****************************************************************************/

static ssize_t vfs_read_fn(void *file, void *buf, size_t len)
{
	struct files_struct *fsp = (struct files_struct *)file;

	return SMB_VFS_READ(fsp, buf, len);
}

static ssize_t vfs_write_fn(void *file, const void *buf, size_t len)
{
	struct files_struct *fsp = (struct files_struct *)file;

	return SMB_VFS_WRITE(fsp, buf, len);
}

SMB_OFF_T vfs_transfer_file(files_struct *in, files_struct *out, SMB_OFF_T n)
{
	return transfer_file_internal((void *)in, (void *)out, n,
				      vfs_read_fn, vfs_write_fn);
}

/*******************************************************************
 A vfs_readdir wrapper which just returns the file name.
********************************************************************/

const char *vfs_readdirname(connection_struct *conn, void *p,
			    SMB_STRUCT_STAT *sbuf, char **talloced)
{
	SMB_STRUCT_DIRENT *ptr= NULL;
	const char *dname;
	char *translated;
	NTSTATUS status;

	if (!p)
		return(NULL);

	ptr = SMB_VFS_READDIR(conn, (DIR *)p, sbuf);
	if (!ptr)
		return(NULL);

	dname = ptr->d_name;


#ifdef NEXT2
	if (telldir(p) < 0)
		return(NULL);
#endif

#ifdef HAVE_BROKEN_READDIR_NAME
	/* using /usr/ucb/cc is BAD */
	dname = dname - 2;
#endif

	status = SMB_VFS_TRANSLATE_NAME(conn, dname, vfs_translate_to_windows,
					talloc_tos(), &translated);
	if (NT_STATUS_EQUAL(status, NT_STATUS_NONE_MAPPED)) {
		*talloced = NULL;
		return dname;
	}
	*talloced = translated;
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}
	return translated;
}

/*******************************************************************
 A wrapper for vfs_chdir().
********************************************************************/

int vfs_ChDir(connection_struct *conn, const char *path)
{
	int res;

	if (!LastDir) {
		LastDir = SMB_STRDUP("");
	}

	if (strcsequal(path,"."))
		return(0);

	if (*path == '/' && strcsequal(LastDir,path))
		return(0);

	DEBUG(4,("vfs_ChDir to %s\n",path));

	res = SMB_VFS_CHDIR(conn,path);
	if (!res) {
		SAFE_FREE(LastDir);
		LastDir = SMB_STRDUP(path);
	}
	return(res);
}

/*******************************************************************
 Return the absolute current directory path - given a UNIX pathname.
 Note that this path is returned in DOS format, not UNIX
 format. Note this can be called with conn == NULL.
********************************************************************/

char *vfs_GetWd(TALLOC_CTX *ctx, connection_struct *conn)
{
        char s[PATH_MAX+1];
	char *result = NULL;
	DATA_BLOB cache_value;
	struct file_id key;
	struct smb_filename *smb_fname_dot = NULL;
	struct smb_filename *smb_fname_full = NULL;
	NTSTATUS status;

	*s = 0;

	if (!lp_getwd_cache()) {
		goto nocache;
	}

	status = create_synthetic_smb_fname(ctx, ".", NULL, NULL,
					    &smb_fname_dot);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	if (SMB_VFS_STAT(conn, smb_fname_dot) == -1) {
		/*
		 * Known to fail for root: the directory may be NFS-mounted
		 * and exported with root_squash (so has no root access).
		 */
		DEBUG(1,("vfs_GetWd: couldn't stat \".\" error %s "
			 "(NFS problem ?)\n", strerror(errno) ));
		goto nocache;
	}

	key = vfs_file_id_from_sbuf(conn, &smb_fname_dot->st);

	if (!memcache_lookup(smbd_memcache(), GETWD_CACHE,
			     data_blob_const(&key, sizeof(key)),
			     &cache_value)) {
		goto nocache;
	}

	SMB_ASSERT((cache_value.length > 0)
		   && (cache_value.data[cache_value.length-1] == '\0'));

	status = create_synthetic_smb_fname(ctx, (char *)cache_value.data,
					    NULL, NULL, &smb_fname_full);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	if ((SMB_VFS_STAT(conn, smb_fname_full) == 0) &&
	    (smb_fname_dot->st.st_ex_dev == smb_fname_full->st.st_ex_dev) &&
	    (smb_fname_dot->st.st_ex_ino == smb_fname_full->st.st_ex_ino) &&
	    (S_ISDIR(smb_fname_dot->st.st_ex_mode))) {
		/*
		 * Ok, we're done
		 */
		result = talloc_strdup(ctx, smb_fname_full->base_name);
		if (result == NULL) {
			errno = ENOMEM;
		}
		goto out;
	}

 nocache:

	/*
	 * We don't have the information to hand so rely on traditional
	 * methods. The very slow getcwd, which spawns a process on some
	 * systems, or the not quite so bad getwd.
	 */

	if (!SMB_VFS_GETWD(conn,s)) {
		DEBUG(0, ("vfs_GetWd: SMB_VFS_GETWD call failed: %s\n",
			  strerror(errno)));
		goto out;
	}

	if (lp_getwd_cache() && VALID_STAT(smb_fname_dot->st)) {
		key = vfs_file_id_from_sbuf(conn, &smb_fname_dot->st);

		memcache_add(smbd_memcache(), GETWD_CACHE,
			     data_blob_const(&key, sizeof(key)),
			     data_blob_const(s, strlen(s)+1));
	}

	result = talloc_strdup(ctx, s);
	if (result == NULL) {
		errno = ENOMEM;
	}

 out:
	TALLOC_FREE(smb_fname_dot);
	TALLOC_FREE(smb_fname_full);
	return result;
}

/*******************************************************************
 Reduce a file name, removing .. elements and checking that
 it is below dir in the heirachy. This uses realpath.
********************************************************************/

NTSTATUS check_reduced_name(connection_struct *conn, const char *fname)
{
	char *resolved_name = NULL;
	bool allow_symlinks = true;
	bool allow_widelinks = false;

	DEBUG(3,("check_reduced_name [%s] [%s]\n", fname, conn->connectpath));

	resolved_name = SMB_VFS_REALPATH(conn,fname);

	if (!resolved_name) {
		switch (errno) {
			case ENOTDIR:
				DEBUG(3,("check_reduced_name: Component not a "
					 "directory in getting realpath for "
					 "%s\n", fname));
				return NT_STATUS_OBJECT_PATH_NOT_FOUND;
			case ENOENT:
			{
				TALLOC_CTX *ctx = talloc_tos();
				char *dir_name = NULL;
				const char *last_component = NULL;
				char *new_name = NULL;

				/* Last component didn't exist.
				   Remove it and try and canonicalise
				   the directory name. */
				if (!parent_dirname(ctx, fname,
						&dir_name,
						&last_component)) {
					return NT_STATUS_NO_MEMORY;
				}

				resolved_name = SMB_VFS_REALPATH(conn,dir_name);
				if (!resolved_name) {
					NTSTATUS status = map_nt_error_from_unix(errno);

					if (errno == ENOENT || errno == ENOTDIR) {
						status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
					}

					DEBUG(3,("check_reduce_name: "
						 "couldn't get realpath for "
						 "%s (%s)\n",
						fname,
						nt_errstr(status)));
					return status;
				}
				new_name = talloc_asprintf(ctx,
						"%s/%s",
						resolved_name,
						last_component);
				if (!new_name) {
					return NT_STATUS_NO_MEMORY;
				}
				SAFE_FREE(resolved_name);
				resolved_name = SMB_STRDUP(new_name);
				if (!resolved_name) {
					return NT_STATUS_NO_MEMORY;
				}
				break;
			}
			default:
				DEBUG(3,("check_reduced_name: couldn't get "
					 "realpath for %s\n", fname));
				return map_nt_error_from_unix(errno);
		}
	}

	DEBUG(10,("check_reduced_name realpath [%s] -> [%s]\n", fname,
		  resolved_name));

	if (*resolved_name != '/') {
		DEBUG(0,("check_reduced_name: realpath doesn't return "
			 "absolute paths !\n"));
		SAFE_FREE(resolved_name);
		return NT_STATUS_OBJECT_NAME_INVALID;
	}

	allow_widelinks = lp_widelinks(SNUM(conn));
	allow_symlinks = lp_symlinks(SNUM(conn));

	/* Common widelinks and symlinks checks. */
	if (!allow_widelinks || !allow_symlinks) {
		const char *conn_rootdir;
		size_t rootdir_len;
		bool matched;

		conn_rootdir = SMB_VFS_CONNECTPATH(conn, fname);
		if (conn_rootdir == NULL) {
			DEBUG(2, ("check_reduced_name: Could not get "
				"conn_rootdir\n"));
			SAFE_FREE(resolved_name);
			return NT_STATUS_ACCESS_DENIED;
		}

		rootdir_len = strlen(conn_rootdir);
		matched = (strncmp(conn_rootdir, resolved_name,
				rootdir_len) == 0);
		if (!matched || (resolved_name[rootdir_len] != '/' &&
				 resolved_name[rootdir_len] != '\0')) {
			DEBUG(2, ("check_reduced_name: Bad access "
				"attempt: %s is a symlink outside the "
				"share path\n", fname));
			DEBUGADD(2, ("conn_rootdir =%s\n", conn_rootdir));
			DEBUGADD(2, ("resolved_name=%s\n", resolved_name));
			SAFE_FREE(resolved_name);
			return NT_STATUS_ACCESS_DENIED;
		}

		/* Extra checks if all symlinks are disallowed. */
		if (!allow_symlinks) {
			/* fname can't have changed in resolved_path. */
			const char *p = &resolved_name[rootdir_len];

			/* *p can be '\0' if fname was "." */
			if (*p == '\0' && ISDOT(fname)) {
				goto out;
			}

			if (*p != '/') {
				DEBUG(2, ("check_reduced_name: logic error (%c) "
					"in resolved_name: %s\n",
					*p,
					fname));
				SAFE_FREE(resolved_name);
				return NT_STATUS_ACCESS_DENIED;
			}

			p++;
			if (strcmp(fname, p)!=0) {
				DEBUG(2, ("check_reduced_name: Bad access "
					"attempt: %s is a symlink\n",
					fname));
				SAFE_FREE(resolved_name);
				return NT_STATUS_ACCESS_DENIED;
			}
		}
	}

  out:

	DEBUG(3,("check_reduced_name: %s reduced to %s\n", fname,
		 resolved_name));
	SAFE_FREE(resolved_name);
	return NT_STATUS_OK;
}

/**
 * XXX: This is temporary and there should be no callers of this once
 * smb_filename is plumbed through all path based operations.
 */
int vfs_stat_smb_fname(struct connection_struct *conn, const char *fname,
		       SMB_STRUCT_STAT *psbuf)
{
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;
	int ret;

	status = create_synthetic_smb_fname_split(talloc_tos(), fname, NULL,
						  &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	if (lp_posix_pathnames()) {
		ret = SMB_VFS_LSTAT(conn, smb_fname);
	} else {
		ret = SMB_VFS_STAT(conn, smb_fname);
	}

	if (ret != -1) {
		*psbuf = smb_fname->st;
	}

	TALLOC_FREE(smb_fname);
	return ret;
}

/**
 * XXX: This is temporary and there should be no callers of this once
 * smb_filename is plumbed through all path based operations.
 */
int vfs_lstat_smb_fname(struct connection_struct *conn, const char *fname,
			SMB_STRUCT_STAT *psbuf)
{
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;
	int ret;

	status = create_synthetic_smb_fname_split(talloc_tos(), fname, NULL,
						  &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_LSTAT(conn, smb_fname);
	if (ret != -1) {
		*psbuf = smb_fname->st;
	}

	TALLOC_FREE(smb_fname);
	return ret;
}

/**
 * Ensure LSTAT is called for POSIX paths.
 */

NTSTATUS vfs_stat_fsp(files_struct *fsp)
{
	int ret;

	if(fsp->fh->fd == -1) {
		if (fsp->posix_open) {
			ret = SMB_VFS_LSTAT(fsp->conn, fsp->fsp_name);
		} else {
			ret = SMB_VFS_STAT(fsp->conn, fsp->fsp_name);
		}
		if (ret == -1) {
			return map_nt_error_from_unix(errno);
		}
	} else {
		if(SMB_VFS_FSTAT(fsp, &fsp->fsp_name->st) != 0) {
			return map_nt_error_from_unix(errno);
		}
	}
	return NT_STATUS_OK;
}

/**
 * Initialize num_streams and streams, then call VFS op streaminfo
 */
NTSTATUS vfs_streaminfo(connection_struct *conn,
			struct files_struct *fsp,
			const char *fname,
			TALLOC_CTX *mem_ctx,
			unsigned int *num_streams,
			struct stream_struct **streams)
{
	*num_streams = 0;
	*streams = NULL;
	return SMB_VFS_STREAMINFO(conn, fsp, fname, mem_ctx, num_streams, streams);
}

/*
  generate a file_id from a stat structure
 */
struct file_id vfs_file_id_from_sbuf(connection_struct *conn, const SMB_STRUCT_STAT *sbuf)
{
	return SMB_VFS_FILE_ID_CREATE(conn, sbuf);
}

int smb_vfs_call_connect(struct vfs_handle_struct *handle,
			 const char *service, const char *user)
{
	VFS_FIND(connect_fn);
	return handle->fns->connect_fn(handle, service, user);
}

void smb_vfs_call_disconnect(struct vfs_handle_struct *handle)
{
	VFS_FIND(disconnect);
	handle->fns->disconnect(handle);
}

uint64_t smb_vfs_call_disk_free(struct vfs_handle_struct *handle,
				const char *path, bool small_query,
				uint64_t *bsize, uint64_t *dfree,
				uint64_t *dsize)
{
	VFS_FIND(disk_free);
	return handle->fns->disk_free(handle, path, small_query, bsize, dfree,
				      dsize);
}

int smb_vfs_call_get_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt)
{
	VFS_FIND(get_quota);
	return handle->fns->get_quota(handle, qtype, id, qt);
}

int smb_vfs_call_set_quota(struct vfs_handle_struct *handle,
			   enum SMB_QUOTA_TYPE qtype, unid_t id,
			   SMB_DISK_QUOTA *qt)
{
	VFS_FIND(set_quota);
	return handle->fns->set_quota(handle, qtype, id, qt);
}

int smb_vfs_call_get_shadow_copy_data(struct vfs_handle_struct *handle,
				      struct files_struct *fsp,
				      struct shadow_copy_data *shadow_copy_data,
				      bool labels)
{
	VFS_FIND(get_shadow_copy_data);
	return handle->fns->get_shadow_copy_data(handle, fsp, shadow_copy_data,
						 labels);
}
int smb_vfs_call_statvfs(struct vfs_handle_struct *handle, const char *path,
			 struct vfs_statvfs_struct *statbuf)
{
	VFS_FIND(statvfs);
	return handle->fns->statvfs(handle, path, statbuf);
}

uint32_t smb_vfs_call_fs_capabilities(struct vfs_handle_struct *handle,
			enum timestamp_set_resolution *p_ts_res)
{
	VFS_FIND(fs_capabilities);
	return handle->fns->fs_capabilities(handle, p_ts_res);
}

SMB_STRUCT_DIR *smb_vfs_call_opendir(struct vfs_handle_struct *handle,
				     const char *fname, const char *mask,
				     uint32 attributes)
{
	VFS_FIND(opendir);
	return handle->fns->opendir(handle, fname, mask, attributes);
}

SMB_STRUCT_DIR *smb_vfs_call_fdopendir(struct vfs_handle_struct *handle,
					struct files_struct *fsp,
					const char *mask,
					uint32 attributes)
{
	VFS_FIND(fdopendir);
	return handle->fns->fdopendir(handle, fsp, mask, attributes);
}

SMB_STRUCT_DIRENT *smb_vfs_call_readdir(struct vfs_handle_struct *handle,
					      SMB_STRUCT_DIR *dirp,
					      SMB_STRUCT_STAT *sbuf)
{
	VFS_FIND(readdir);
	return handle->fns->readdir(handle, dirp, sbuf);
}

void smb_vfs_call_seekdir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp, long offset)
{
	VFS_FIND(seekdir);
	handle->fns->seekdir(handle, dirp, offset);
}

long smb_vfs_call_telldir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dirp)
{
	VFS_FIND(telldir);
	return handle->fns->telldir(handle, dirp);
}

void smb_vfs_call_rewind_dir(struct vfs_handle_struct *handle,
			     SMB_STRUCT_DIR *dirp)
{
	VFS_FIND(rewind_dir);
	handle->fns->rewind_dir(handle, dirp);
}

int smb_vfs_call_mkdir(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode)
{
	VFS_FIND(mkdir);
	return handle->fns->mkdir(handle, path, mode);
}

int smb_vfs_call_rmdir(struct vfs_handle_struct *handle, const char *path)
{
	VFS_FIND(rmdir);
	return handle->fns->rmdir(handle, path);
}

int smb_vfs_call_closedir(struct vfs_handle_struct *handle,
			  SMB_STRUCT_DIR *dir)
{
	VFS_FIND(closedir);
	return handle->fns->closedir(handle, dir);
}

void smb_vfs_call_init_search_op(struct vfs_handle_struct *handle,
				 SMB_STRUCT_DIR *dirp)
{
	VFS_FIND(init_search_op);
	handle->fns->init_search_op(handle, dirp);
}

int smb_vfs_call_open(struct vfs_handle_struct *handle,
		      struct smb_filename *smb_fname, struct files_struct *fsp,
		      int flags, mode_t mode)
{
	VFS_FIND(open_fn);
	return handle->fns->open_fn(handle, smb_fname, fsp, flags, mode);
}

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
				  int *pinfo)
{
	VFS_FIND(create_file);
	return handle->fns->create_file(
		handle, req, root_dir_fid, smb_fname, access_mask,
		share_access, create_disposition, create_options,
		file_attributes, oplock_request, allocation_size,
		private_flags, sd, ea_list,
		result, pinfo);
}

int smb_vfs_call_close_fn(struct vfs_handle_struct *handle,
			  struct files_struct *fsp)
{
	VFS_FIND(close_fn);
	return handle->fns->close_fn(handle, fsp);
}

ssize_t smb_vfs_call_vfs_read(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, void *data, size_t n)
{
	VFS_FIND(vfs_read);
	return handle->fns->vfs_read(handle, fsp, data, n);
}

ssize_t smb_vfs_call_pread(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, void *data, size_t n,
			   SMB_OFF_T offset)
{
	VFS_FIND(pread);
	return handle->fns->pread(handle, fsp, data, n, offset);
}

ssize_t smb_vfs_call_write(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, const void *data,
			   size_t n)
{
	VFS_FIND(write);
	return handle->fns->write(handle, fsp, data, n);
}

ssize_t smb_vfs_call_pwrite(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, const void *data,
			    size_t n, SMB_OFF_T offset)
{
	VFS_FIND(pwrite);
	return handle->fns->pwrite(handle, fsp, data, n, offset);
}

SMB_OFF_T smb_vfs_call_lseek(struct vfs_handle_struct *handle,
			     struct files_struct *fsp, SMB_OFF_T offset,
			     int whence)
{
	VFS_FIND(lseek);
	return handle->fns->lseek(handle, fsp, offset, whence);
}

ssize_t smb_vfs_call_sendfile(struct vfs_handle_struct *handle, int tofd,
			      files_struct *fromfsp, const DATA_BLOB *header,
			      SMB_OFF_T offset, size_t count)
{
	VFS_FIND(sendfile);
	return handle->fns->sendfile(handle, tofd, fromfsp, header, offset,
				     count);
}

ssize_t smb_vfs_call_recvfile(struct vfs_handle_struct *handle, int fromfd,
			      files_struct *tofsp, SMB_OFF_T offset,
			      size_t count)
{
	VFS_FIND(recvfile);
	return handle->fns->recvfile(handle, fromfd, tofsp, offset, count);
}

int smb_vfs_call_rename(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname_src,
			const struct smb_filename *smb_fname_dst)
{
	VFS_FIND(rename);
	return handle->fns->rename(handle, smb_fname_src, smb_fname_dst);
}

int smb_vfs_call_fsync(struct vfs_handle_struct *handle,
		       struct files_struct *fsp)
{
	VFS_FIND(fsync);
	return handle->fns->fsync(handle, fsp);
}

int smb_vfs_call_stat(struct vfs_handle_struct *handle,
		      struct smb_filename *smb_fname)
{
	VFS_FIND(stat);
	return handle->fns->stat(handle, smb_fname);
}

int smb_vfs_call_fstat(struct vfs_handle_struct *handle,
		       struct files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	VFS_FIND(fstat);
	return handle->fns->fstat(handle, fsp, sbuf);
}

int smb_vfs_call_lstat(struct vfs_handle_struct *handle,
		       struct smb_filename *smb_filename)
{
	VFS_FIND(lstat);
	return handle->fns->lstat(handle, smb_filename);
}

uint64_t smb_vfs_call_get_alloc_size(struct vfs_handle_struct *handle,
				     struct files_struct *fsp,
				     const SMB_STRUCT_STAT *sbuf)
{
	VFS_FIND(get_alloc_size);
	return handle->fns->get_alloc_size(handle, fsp, sbuf);
}

int smb_vfs_call_unlink(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname)
{
	VFS_FIND(unlink);
	return handle->fns->unlink(handle, smb_fname);
}

int smb_vfs_call_chmod(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode)
{
	VFS_FIND(chmod);
	return handle->fns->chmod(handle, path, mode);
}

int smb_vfs_call_fchmod(struct vfs_handle_struct *handle,
			struct files_struct *fsp, mode_t mode)
{
	VFS_FIND(fchmod);
	return handle->fns->fchmod(handle, fsp, mode);
}

int smb_vfs_call_chown(struct vfs_handle_struct *handle, const char *path,
		       uid_t uid, gid_t gid)
{
	VFS_FIND(chown);
	return handle->fns->chown(handle, path, uid, gid);
}

int smb_vfs_call_fchown(struct vfs_handle_struct *handle,
			struct files_struct *fsp, uid_t uid, gid_t gid)
{
	VFS_FIND(fchown);
	return handle->fns->fchown(handle, fsp, uid, gid);
}

int smb_vfs_call_lchown(struct vfs_handle_struct *handle, const char *path,
			uid_t uid, gid_t gid)
{
	VFS_FIND(lchown);
	return handle->fns->lchown(handle, path, uid, gid);
}

NTSTATUS vfs_chown_fsp(files_struct *fsp, uid_t uid, gid_t gid)
{
	int ret;
	bool as_root = false;
	const char *path;
	char *saved_dir = NULL;
	char *parent_dir = NULL;
	NTSTATUS status;

	if (fsp->fh->fd != -1) {
		/* Try fchown. */
		ret = SMB_VFS_FCHOWN(fsp, uid, gid);
		if (ret == 0) {
			return NT_STATUS_OK;
		}
		if (ret == -1 && errno != ENOSYS) {
			return map_nt_error_from_unix(errno);
		}
	}

	as_root = (geteuid() == 0);

	if (as_root) {
		/*
		 * We are being asked to chown as root. Make
		 * sure we chdir() into the path to pin it,
		 * and always act using lchown to ensure we
		 * don't deref any symbolic links.
		 */
		const char *final_component = NULL;
		struct smb_filename local_fname;

		saved_dir = vfs_GetWd(talloc_tos(),fsp->conn);
		if (!saved_dir) {
			status = map_nt_error_from_unix(errno);
			DEBUG(0,("vfs_chown_fsp: failed to get "
				"current working directory. Error was %s\n",
				strerror(errno)));
			return status;
		}

		if (!parent_dirname(talloc_tos(),
				fsp->fsp_name->base_name,
				&parent_dir,
				&final_component)) {
			return NT_STATUS_NO_MEMORY;
		}

		/* cd into the parent dir to pin it. */
		ret = vfs_ChDir(fsp->conn, parent_dir);
		if (ret == -1) {
			return map_nt_error_from_unix(errno);
		}

		ZERO_STRUCT(local_fname);
		local_fname.base_name = CONST_DISCARD(char *,final_component);

		/* Must use lstat here. */
		ret = SMB_VFS_LSTAT(fsp->conn, &local_fname);
		if (ret == -1) {
			status = map_nt_error_from_unix(errno);
			goto out;
		}

		/* Ensure it matches the fsp stat. */
		if (!check_same_stat(&local_fname.st, &fsp->fsp_name->st)) {
                        status = NT_STATUS_ACCESS_DENIED;
			goto out;
                }
                path = final_component;
        } else {
                path = fsp->fsp_name->base_name;
        }

	if (fsp->posix_open || as_root) {
		ret = SMB_VFS_LCHOWN(fsp->conn,
			path,
			uid, gid);
	} else {
		ret = SMB_VFS_CHOWN(fsp->conn,
			path,
			uid, gid);
	}

	if (ret == 0) {
		status = NT_STATUS_OK;
	} else {
		status = map_nt_error_from_unix(errno);
	}

  out:

	if (as_root) {
		vfs_ChDir(fsp->conn,saved_dir);
		TALLOC_FREE(saved_dir);
		TALLOC_FREE(parent_dir);
	}
	return status;
}

int smb_vfs_call_chdir(struct vfs_handle_struct *handle, const char *path)
{
	VFS_FIND(chdir);
	return handle->fns->chdir(handle, path);
}

char *smb_vfs_call_getwd(struct vfs_handle_struct *handle, char *buf)
{
	VFS_FIND(getwd);
	return handle->fns->getwd(handle, buf);
}

int smb_vfs_call_ntimes(struct vfs_handle_struct *handle,
			const struct smb_filename *smb_fname,
			struct smb_file_time *ft)
{
	VFS_FIND(ntimes);
	return handle->fns->ntimes(handle, smb_fname, ft);
}

int smb_vfs_call_ftruncate(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, SMB_OFF_T offset)
{
	VFS_FIND(ftruncate);
	return handle->fns->ftruncate(handle, fsp, offset);
}

int smb_vfs_call_fallocate(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				enum vfs_fallocate_mode mode,
				SMB_OFF_T offset,
				SMB_OFF_T len)
{
	VFS_FIND(fallocate);
	return handle->fns->fallocate(handle, fsp, mode, offset, len);
}

int smb_vfs_call_kernel_flock(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, uint32 share_mode,
			      uint32_t access_mask)
{
	VFS_FIND(kernel_flock);
	return handle->fns->kernel_flock(handle, fsp, share_mode,
					 access_mask);
}

int smb_vfs_call_linux_setlease(struct vfs_handle_struct *handle,
				struct files_struct *fsp, int leasetype)
{
	VFS_FIND(linux_setlease);
	return handle->fns->linux_setlease(handle, fsp, leasetype);
}

int smb_vfs_call_symlink(struct vfs_handle_struct *handle, const char *oldpath,
			 const char *newpath)
{
	VFS_FIND(symlink);
	return handle->fns->symlink(handle, oldpath, newpath);
}

int smb_vfs_call_vfs_readlink(struct vfs_handle_struct *handle,
			      const char *path, char *buf, size_t bufsiz)
{
	VFS_FIND(vfs_readlink);
	return handle->fns->vfs_readlink(handle, path, buf, bufsiz);
}

int smb_vfs_call_link(struct vfs_handle_struct *handle, const char *oldpath,
		      const char *newpath)
{
	VFS_FIND(link);
	return handle->fns->link(handle, oldpath, newpath);
}

int smb_vfs_call_mknod(struct vfs_handle_struct *handle, const char *path,
		       mode_t mode, SMB_DEV_T dev)
{
	VFS_FIND(mknod);
	return handle->fns->mknod(handle, path, mode, dev);
}

char *smb_vfs_call_realpath(struct vfs_handle_struct *handle, const char *path)
{
	VFS_FIND(realpath);
	return handle->fns->realpath(handle, path);
}

NTSTATUS smb_vfs_call_notify_watch(struct vfs_handle_struct *handle,
				   struct sys_notify_context *ctx,
				   struct notify_entry *e,
				   void (*callback)(struct sys_notify_context *ctx,
						    void *private_data,
						    struct notify_event *ev),
				   void *private_data, void *handle_p)
{
	VFS_FIND(notify_watch);
	return handle->fns->notify_watch(handle, ctx, e, callback,
					 private_data, handle_p);
}

int smb_vfs_call_chflags(struct vfs_handle_struct *handle, const char *path,
			 unsigned int flags)
{
	VFS_FIND(chflags);
	return handle->fns->chflags(handle, path, flags);
}

struct file_id smb_vfs_call_file_id_create(struct vfs_handle_struct *handle,
					   const SMB_STRUCT_STAT *sbuf)
{
	VFS_FIND(file_id_create);
	return handle->fns->file_id_create(handle, sbuf);
}

NTSTATUS smb_vfs_call_streaminfo(struct vfs_handle_struct *handle,
				 struct files_struct *fsp,
				 const char *fname,
				 TALLOC_CTX *mem_ctx,
				 unsigned int *num_streams,
				 struct stream_struct **streams)
{
	VFS_FIND(streaminfo);
	return handle->fns->streaminfo(handle, fsp, fname, mem_ctx,
				       num_streams, streams);
}

int smb_vfs_call_get_real_filename(struct vfs_handle_struct *handle,
				   const char *path, const char *name,
				   TALLOC_CTX *mem_ctx, char **found_name)
{
	VFS_FIND(get_real_filename);
	return handle->fns->get_real_filename(handle, path, name, mem_ctx,
					      found_name);
}

const char *smb_vfs_call_connectpath(struct vfs_handle_struct *handle,
				     const char *filename)
{
	VFS_FIND(connectpath);
	return handle->fns->connectpath(handle, filename);
}

bool smb_vfs_call_strict_lock(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      struct lock_struct *plock)
{
	VFS_FIND(strict_lock);
	return handle->fns->strict_lock(handle, fsp, plock);
}

void smb_vfs_call_strict_unlock(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				struct lock_struct *plock)
{
	VFS_FIND(strict_unlock);
	handle->fns->strict_unlock(handle, fsp, plock);
}

NTSTATUS smb_vfs_call_translate_name(struct vfs_handle_struct *handle,
				     const char *name,
				     enum vfs_translate_direction direction,
				     TALLOC_CTX *mem_ctx,
				     char **mapped_name)
{
	VFS_FIND(translate_name);
	return handle->fns->translate_name(handle, name, direction, mem_ctx,
					   mapped_name);
}

NTSTATUS smb_vfs_call_fget_nt_acl(struct vfs_handle_struct *handle,
				  struct files_struct *fsp,
				  uint32 security_info,
				  struct security_descriptor **ppdesc)
{
	VFS_FIND(fget_nt_acl);
	return handle->fns->fget_nt_acl(handle, fsp, security_info,
					ppdesc);
}

NTSTATUS smb_vfs_call_get_nt_acl(struct vfs_handle_struct *handle,
				 const char *name,
				 uint32 security_info,
				 struct security_descriptor **ppdesc)
{
	VFS_FIND(get_nt_acl);
	return handle->fns->get_nt_acl(handle, name, security_info, ppdesc);
}

NTSTATUS smb_vfs_call_fset_nt_acl(struct vfs_handle_struct *handle,
				  struct files_struct *fsp,
				  uint32 security_info_sent,
				  const struct security_descriptor *psd)
{
	VFS_FIND(fset_nt_acl);
	return handle->fns->fset_nt_acl(handle, fsp, security_info_sent, psd);
}

int smb_vfs_call_chmod_acl(struct vfs_handle_struct *handle, const char *name,
			   mode_t mode)
{
	VFS_FIND(chmod_acl);
	return handle->fns->chmod_acl(handle, name, mode);
}

int smb_vfs_call_fchmod_acl(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, mode_t mode)
{
	VFS_FIND(fchmod_acl);
	return handle->fns->fchmod_acl(handle, fsp, mode);
}

int smb_vfs_call_sys_acl_get_entry(struct vfs_handle_struct *handle,
				   SMB_ACL_T theacl, int entry_id,
				   SMB_ACL_ENTRY_T *entry_p)
{
	VFS_FIND(sys_acl_get_entry);
	return handle->fns->sys_acl_get_entry(handle, theacl, entry_id,
					      entry_p);
}

int smb_vfs_call_sys_acl_get_tag_type(struct vfs_handle_struct *handle,
				      SMB_ACL_ENTRY_T entry_d,
				      SMB_ACL_TAG_T *tag_type_p)
{
	VFS_FIND(sys_acl_get_tag_type);
	return handle->fns->sys_acl_get_tag_type(handle, entry_d, tag_type_p);
}

int smb_vfs_call_sys_acl_get_permset(struct vfs_handle_struct *handle,
				     SMB_ACL_ENTRY_T entry_d,
				     SMB_ACL_PERMSET_T *permset_p)
{
	VFS_FIND(sys_acl_get_permset);
	return handle->fns->sys_acl_get_permset(handle, entry_d, permset_p);
}

void * smb_vfs_call_sys_acl_get_qualifier(struct vfs_handle_struct *handle,
					  SMB_ACL_ENTRY_T entry_d)
{
	VFS_FIND(sys_acl_get_qualifier);
	return handle->fns->sys_acl_get_qualifier(handle, entry_d);
}

SMB_ACL_T smb_vfs_call_sys_acl_get_file(struct vfs_handle_struct *handle,
					const char *path_p,
					SMB_ACL_TYPE_T type)
{
	VFS_FIND(sys_acl_get_file);
	return handle->fns->sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T smb_vfs_call_sys_acl_get_fd(struct vfs_handle_struct *handle,
				      struct files_struct *fsp)
{
	VFS_FIND(sys_acl_get_fd);
	return handle->fns->sys_acl_get_fd(handle, fsp);
}

int smb_vfs_call_sys_acl_clear_perms(struct vfs_handle_struct *handle,
				     SMB_ACL_PERMSET_T permset)
{
	VFS_FIND(sys_acl_clear_perms);
	return handle->fns->sys_acl_clear_perms(handle, permset);
}

int smb_vfs_call_sys_acl_add_perm(struct vfs_handle_struct *handle,
				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm)
{
	VFS_FIND(sys_acl_add_perm);
	return handle->fns->sys_acl_add_perm(handle, permset, perm);
}

char * smb_vfs_call_sys_acl_to_text(struct vfs_handle_struct *handle,
				    SMB_ACL_T theacl, ssize_t *plen)
{
	VFS_FIND(sys_acl_to_text);
	return handle->fns->sys_acl_to_text(handle, theacl, plen);
}

SMB_ACL_T smb_vfs_call_sys_acl_init(struct vfs_handle_struct *handle,
				    int count)
{
	VFS_FIND(sys_acl_init);
	return handle->fns->sys_acl_init(handle, count);
}

int smb_vfs_call_sys_acl_create_entry(struct vfs_handle_struct *handle,
				      SMB_ACL_T *pacl, SMB_ACL_ENTRY_T *pentry)
{
	VFS_FIND(sys_acl_create_entry);
	return handle->fns->sys_acl_create_entry(handle, pacl, pentry);
}

int smb_vfs_call_sys_acl_set_tag_type(struct vfs_handle_struct *handle,
				      SMB_ACL_ENTRY_T entry,
				      SMB_ACL_TAG_T tagtype)
{
	VFS_FIND(sys_acl_set_tag_type);
	return handle->fns->sys_acl_set_tag_type(handle, entry, tagtype);
}

int smb_vfs_call_sys_acl_set_qualifier(struct vfs_handle_struct *handle,
				       SMB_ACL_ENTRY_T entry, void *qual)
{
	VFS_FIND(sys_acl_set_qualifier);
	return handle->fns->sys_acl_set_qualifier(handle, entry, qual);
}

int smb_vfs_call_sys_acl_set_permset(struct vfs_handle_struct *handle,
				     SMB_ACL_ENTRY_T entry,
				     SMB_ACL_PERMSET_T permset)
{
	VFS_FIND(sys_acl_set_permset);
	return handle->fns->sys_acl_set_permset(handle, entry, permset);
}

int smb_vfs_call_sys_acl_valid(struct vfs_handle_struct *handle,
			       SMB_ACL_T theacl)
{
	VFS_FIND(sys_acl_valid);
	return handle->fns->sys_acl_valid(handle, theacl);
}

int smb_vfs_call_sys_acl_set_file(struct vfs_handle_struct *handle,
				  const char *name, SMB_ACL_TYPE_T acltype,
				  SMB_ACL_T theacl)
{
	VFS_FIND(sys_acl_set_file);
	return handle->fns->sys_acl_set_file(handle, name, acltype, theacl);
}

int smb_vfs_call_sys_acl_set_fd(struct vfs_handle_struct *handle,
				struct files_struct *fsp, SMB_ACL_T theacl)
{
	VFS_FIND(sys_acl_set_fd);
	return handle->fns->sys_acl_set_fd(handle, fsp, theacl);
}

int smb_vfs_call_sys_acl_delete_def_file(struct vfs_handle_struct *handle,
					 const char *path)
{
	VFS_FIND(sys_acl_delete_def_file);
	return handle->fns->sys_acl_delete_def_file(handle, path);
}

int smb_vfs_call_sys_acl_get_perm(struct vfs_handle_struct *handle,
				  SMB_ACL_PERMSET_T permset,
				  SMB_ACL_PERM_T perm)
{
	VFS_FIND(sys_acl_get_perm);
	return handle->fns->sys_acl_get_perm(handle, permset, perm);
}

int smb_vfs_call_sys_acl_free_text(struct vfs_handle_struct *handle,
				   char *text)
{
	VFS_FIND(sys_acl_free_text);
	return handle->fns->sys_acl_free_text(handle, text);
}

int smb_vfs_call_sys_acl_free_acl(struct vfs_handle_struct *handle,
				  SMB_ACL_T posix_acl)
{
	VFS_FIND(sys_acl_free_acl);
	return handle->fns->sys_acl_free_acl(handle, posix_acl);
}

int smb_vfs_call_sys_acl_free_qualifier(struct vfs_handle_struct *handle,
					void *qualifier, SMB_ACL_TAG_T tagtype)
{
	VFS_FIND(sys_acl_free_qualifier);
	return handle->fns->sys_acl_free_qualifier(handle, qualifier, tagtype);
}

ssize_t smb_vfs_call_getxattr(struct vfs_handle_struct *handle,
			      const char *path, const char *name, void *value,
			      size_t size)
{
	VFS_FIND(getxattr);
	return handle->fns->getxattr(handle, path, name, value, size);
}

ssize_t smb_vfs_call_lgetxattr(struct vfs_handle_struct *handle,
			       const char *path, const char *name, void *value,
			       size_t size)
{
	VFS_FIND(lgetxattr);
	return handle->fns->lgetxattr(handle, path, name, value, size);
}

ssize_t smb_vfs_call_fgetxattr(struct vfs_handle_struct *handle,
			       struct files_struct *fsp, const char *name,
			       void *value, size_t size)
{
	VFS_FIND(fgetxattr);
	return handle->fns->fgetxattr(handle, fsp, name, value, size);
}

ssize_t smb_vfs_call_listxattr(struct vfs_handle_struct *handle,
			       const char *path, char *list, size_t size)
{
	VFS_FIND(listxattr);
	return handle->fns->listxattr(handle, path, list, size);
}

ssize_t smb_vfs_call_llistxattr(struct vfs_handle_struct *handle,
				const char *path, char *list, size_t size)
{
	VFS_FIND(llistxattr);
	return handle->fns->llistxattr(handle, path, list, size);
}

ssize_t smb_vfs_call_flistxattr(struct vfs_handle_struct *handle,
				struct files_struct *fsp, char *list,
				size_t size)
{
	VFS_FIND(flistxattr);
	return handle->fns->flistxattr(handle, fsp, list, size);
}

int smb_vfs_call_removexattr(struct vfs_handle_struct *handle,
			     const char *path, const char *name)
{
	VFS_FIND(removexattr);
	return handle->fns->removexattr(handle, path, name);
}

int smb_vfs_call_lremovexattr(struct vfs_handle_struct *handle,
			      const char *path, const char *name)
{
	VFS_FIND(lremovexattr);
	return handle->fns->lremovexattr(handle, path, name);
}

int smb_vfs_call_fremovexattr(struct vfs_handle_struct *handle,
			      struct files_struct *fsp, const char *name)
{
	VFS_FIND(fremovexattr);
	return handle->fns->fremovexattr(handle, fsp, name);
}

int smb_vfs_call_setxattr(struct vfs_handle_struct *handle, const char *path,
			  const char *name, const void *value, size_t size,
			  int flags)
{
	VFS_FIND(setxattr);
	return handle->fns->setxattr(handle, path, name, value, size, flags);
}

int smb_vfs_call_lsetxattr(struct vfs_handle_struct *handle, const char *path,
			   const char *name, const void *value, size_t size,
			   int flags)
{
	VFS_FIND(lsetxattr);
	return handle->fns->lsetxattr(handle, path, name, value, size, flags);
}

int smb_vfs_call_fsetxattr(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, const char *name,
			   const void *value, size_t size, int flags)
{
	VFS_FIND(fsetxattr);
	return handle->fns->fsetxattr(handle, fsp, name, value, size, flags);
}

int smb_vfs_call_aio_read(struct vfs_handle_struct *handle,
			  struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_read);
	return handle->fns->aio_read(handle, fsp, aiocb);
}

int smb_vfs_call_aio_write(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_write);
	return handle->fns->aio_write(handle, fsp, aiocb);
}

ssize_t smb_vfs_call_aio_return_fn(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_return_fn);
	return handle->fns->aio_return_fn(handle, fsp, aiocb);
}

int smb_vfs_call_aio_cancel(struct vfs_handle_struct *handle,
			    struct files_struct *fsp, SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_cancel);
	return handle->fns->aio_cancel(handle, fsp, aiocb);
}

int smb_vfs_call_aio_error_fn(struct vfs_handle_struct *handle,
			      struct files_struct *fsp,
			      SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_error_fn);
	return handle->fns->aio_error_fn(handle, fsp, aiocb);
}

int smb_vfs_call_aio_fsync(struct vfs_handle_struct *handle,
			   struct files_struct *fsp, int op,
			   SMB_STRUCT_AIOCB *aiocb)
{
	VFS_FIND(aio_fsync);
	return handle->fns->aio_fsync(handle, fsp, op, aiocb);
}

int smb_vfs_call_aio_suspend(struct vfs_handle_struct *handle,
			     struct files_struct *fsp,
			     const SMB_STRUCT_AIOCB * const aiocb[], int n,
			     const struct timespec *timeout)
{
	VFS_FIND(aio_suspend);
	return handle->fns->aio_suspend(handle, fsp, aiocb, n, timeout);
}

bool smb_vfs_call_aio_force(struct vfs_handle_struct *handle,
			    struct files_struct *fsp)
{
	VFS_FIND(aio_force);
	return handle->fns->aio_force(handle, fsp);
}

bool smb_vfs_call_is_offline(struct vfs_handle_struct *handle,
			     const struct smb_filename *fname,
			     SMB_STRUCT_STAT *sbuf)
{
	VFS_FIND(is_offline);
	return handle->fns->is_offline(handle, fname, sbuf);
}

int smb_vfs_call_set_offline(struct vfs_handle_struct *handle,
                             const struct smb_filename *fname)
{
	VFS_FIND(set_offline);
	return handle->fns->set_offline(handle, fname);
}
