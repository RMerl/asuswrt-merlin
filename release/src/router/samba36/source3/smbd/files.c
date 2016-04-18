/* 
   Unix SMB/CIFS implementation.
   Files[] structure handling
   Copyright (C) Andrew Tridgell 1998

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

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "libcli/security/security.h"
#include "util_tdb.h"
#include "lib/util/bitmap.h"

#define VALID_FNUM(fnum)   (((fnum) >= 0) && ((fnum) < real_max_open_files))

#define FILE_HANDLE_OFFSET 0x1000

/****************************************************************************
 Return a unique number identifying this fsp over the life of this pid,
 and try to make it as globally unique as possible.
 See bug #8995 for the details.
****************************************************************************/

static unsigned long get_gen_count(struct smbd_server_connection *sconn)
{
	/*
	 * While fsp->fh->gen_id is 'unsigned long' currently
	 * (which might by 8 bytes),
	 * there's some oplock code which truncates it to
	 * uint32_t(using IVAL()).
	 */
	if (sconn->file_gen_counter == 0) {
		sconn->file_gen_counter = generate_random();
	}
	sconn->file_gen_counter += 1;
	if (sconn->file_gen_counter >= UINT32_MAX) {
		sconn->file_gen_counter = 0;
	}
	if (sconn->file_gen_counter == 0) {
		sconn->file_gen_counter += 1;
	}
	return sconn->file_gen_counter;
}

/****************************************************************************
 Find first available file slot.
****************************************************************************/

NTSTATUS file_new(struct smb_request *req, connection_struct *conn,
		  files_struct **result)
{
	struct smbd_server_connection *sconn = conn->sconn;
	int i;
	files_struct *fsp;
	NTSTATUS status;

	/* we want to give out file handles differently on each new
	   connection because of a common bug in MS clients where they try to
	   reuse a file descriptor from an earlier smb connection. This code
	   increases the chance that the errant client will get an error rather
	   than causing corruption */
	if (sconn->first_file == 0) {
		sconn->first_file = (sys_getpid() ^ (int)time(NULL));
		sconn->first_file %= sconn->real_max_open_files;
	}

	/* TODO: Port the id-tree implementation from Samba4 */

	i = bitmap_find(sconn->file_bmap, sconn->first_file);
	if (i == -1) {
		DEBUG(0,("ERROR! Out of file structures\n"));
		/* TODO: We have to unconditionally return a DOS error here,
		 * W2k3 even returns ERRDOS/ERRnofids for ntcreate&x with
		 * NTSTATUS negotiated */
		return NT_STATUS_TOO_MANY_OPENED_FILES;
	}

	/*
	 * Make a child of the connection_struct as an fsp can't exist
	 * independent of a connection.
	 */
	fsp = talloc_zero(conn, struct files_struct);
	if (!fsp) {
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * This can't be a child of fsp because the file_handle can be ref'd
	 * when doing a dos/fcb open, which will then share the file_handle
	 * across multiple fsps.
	 */
	fsp->fh = talloc_zero(conn, struct fd_handle);
	if (!fsp->fh) {
		TALLOC_FREE(fsp);
		return NT_STATUS_NO_MEMORY;
	}

	fsp->fh->ref_count = 1;
	fsp->fh->fd = -1;

	fsp->conn = conn;
	fsp->fh->gen_id = get_gen_count(sconn);
	GetTimeOfDay(&fsp->open_time);

	sconn->first_file = (i+1) % (sconn->real_max_open_files);

	bitmap_set(sconn->file_bmap, i);
	sconn->files_used += 1;

	fsp->fnum = i + FILE_HANDLE_OFFSET;
	SMB_ASSERT(fsp->fnum < 65536);

	/*
	 * Create an smb_filename with "" for the base_name.  There are very
	 * few NULL checks, so make sure it's initialized with something. to
	 * be safe until an audit can be done.
	 */
	status = create_synthetic_smb_fname(fsp, "", NULL, NULL,
					    &fsp->fsp_name);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(fsp);
		TALLOC_FREE(fsp->fh);
	}

	DLIST_ADD(sconn->files, fsp);

	DEBUG(5,("allocated file structure %d, fnum = %d (%d used)\n",
		 i, fsp->fnum, sconn->files_used));

	if (req != NULL) {
		req->chain_fsp = fsp;
	}

	/* A new fsp invalidates the positive and
	  negative fsp_fi_cache as the new fsp is pushed
	  at the start of the list and we search from
	  a cache hit to the *end* of the list. */

	ZERO_STRUCT(sconn->fsp_fi_cache);

	conn->num_files_open++;

	*result = fsp;
	return NT_STATUS_OK;
}

/****************************************************************************
 Close all open files for a connection.
****************************************************************************/

void file_close_conn(connection_struct *conn)
{
	files_struct *fsp, *next;

	for (fsp=conn->sconn->files; fsp; fsp=next) {
		next = fsp->next;
		if (fsp->conn == conn) {
			close_file(NULL, fsp, SHUTDOWN_CLOSE);
		}
	}
}

/****************************************************************************
 Close all open files for a pid and a vuid.
****************************************************************************/

void file_close_pid(struct smbd_server_connection *sconn, uint16 smbpid,
		    int vuid)
{
	files_struct *fsp, *next;

	for (fsp=sconn->files;fsp;fsp=next) {
		next = fsp->next;
		if ((fsp->file_pid == smbpid) && (fsp->vuid == vuid)) {
			close_file(NULL, fsp, SHUTDOWN_CLOSE);
		}
	}
}

/****************************************************************************
 Initialise file structures.
****************************************************************************/

bool file_init(struct smbd_server_connection *sconn)
{
	int request_max_open_files = lp_max_open_files();
	int real_lim;

	/*
	 * Set the max_open files to be the requested
	 * max plus a fudgefactor to allow for the extra
	 * fd's we need such as log files etc...
	 */
	real_lim = set_maxfiles(request_max_open_files + MAX_OPEN_FUDGEFACTOR);

	sconn->real_max_open_files = real_lim - MAX_OPEN_FUDGEFACTOR;

	if (sconn->real_max_open_files + FILE_HANDLE_OFFSET + MAX_OPEN_PIPES
	    > 65536)
		sconn->real_max_open_files =
			65536 - FILE_HANDLE_OFFSET - MAX_OPEN_PIPES;

	if(sconn->real_max_open_files != request_max_open_files) {
		DEBUG(1, ("file_init: Information only: requested %d "
			  "open files, %d are available.\n",
			  request_max_open_files, sconn->real_max_open_files));
	}

	SMB_ASSERT(sconn->real_max_open_files > 100);

	sconn->file_bmap = bitmap_talloc(sconn, sconn->real_max_open_files);

	if (!sconn->file_bmap) {
		return false;
	}
	return true;
}

/****************************************************************************
 Close files open by a specified vuid.
****************************************************************************/

void file_close_user(struct smbd_server_connection *sconn, int vuid)
{
	files_struct *fsp, *next;

	for (fsp=sconn->files; fsp; fsp=next) {
		next=fsp->next;
		if (fsp->vuid == vuid) {
			close_file(NULL, fsp, SHUTDOWN_CLOSE);
		}
	}
}

/*
 * Walk the files table until "fn" returns non-NULL
 */

struct files_struct *files_forall(
	struct smbd_server_connection *sconn,
	struct files_struct *(*fn)(struct files_struct *fsp,
				   void *private_data),
	void *private_data)
{
	struct files_struct *fsp, *next;

	for (fsp = sconn->files; fsp; fsp = next) {
		struct files_struct *ret;
		next = fsp->next;
		ret = fn(fsp, private_data);
		if (ret != NULL) {
			return ret;
		}
	}
	return NULL;
}

/****************************************************************************
 Find a fsp given a file descriptor.
****************************************************************************/

files_struct *file_find_fd(struct smbd_server_connection *sconn, int fd)
{
	int count=0;
	files_struct *fsp;

	for (fsp=sconn->files; fsp; fsp=fsp->next,count++) {
		if (fsp->fh->fd == fd) {
			if (count > 10) {
				DLIST_PROMOTE(sconn->files, fsp);
			}
			return fsp;
		}
	}

	return NULL;
}

/****************************************************************************
 Find a fsp given a device, inode and file_id.
****************************************************************************/

files_struct *file_find_dif(struct smbd_server_connection *sconn,
			    struct file_id id, unsigned long gen_id)
{
	int count=0;
	files_struct *fsp;

	if (gen_id == 0) {
		return NULL;
	}

	for (fsp=sconn->files; fsp; fsp=fsp->next,count++) {
		/* We can have a fsp->fh->fd == -1 here as it could be a stat open. */
		if (file_id_equal(&fsp->file_id, &id) &&
		    fsp->fh->gen_id == gen_id ) {
			if (count > 10) {
				DLIST_PROMOTE(sconn->files, fsp);
			}
			/* Paranoia check. */
			if ((fsp->fh->fd == -1) &&
			    (fsp->oplock_type != NO_OPLOCK) &&
			    (fsp->oplock_type != FAKE_LEVEL_II_OPLOCK)) {
				DEBUG(0,("file_find_dif: file %s file_id = "
					 "%s, gen = %u oplock_type = %u is a "
					 "stat open with oplock type !\n",
					 fsp_str_dbg(fsp),
					 file_id_string_tos(&fsp->file_id),
					 (unsigned int)fsp->fh->gen_id,
					 (unsigned int)fsp->oplock_type ));
				smb_panic("file_find_dif");
			}
			return fsp;
		}
	}

	return NULL;
}

/****************************************************************************
 Find the first fsp given a device and inode.
 We use a singleton cache here to speed up searching from getfilepathinfo
 calls.
****************************************************************************/

files_struct *file_find_di_first(struct smbd_server_connection *sconn,
				 struct file_id id)
{
	files_struct *fsp;

	if (file_id_equal(&sconn->fsp_fi_cache.id, &id)) {
		/* Positive or negative cache hit. */
		return sconn->fsp_fi_cache.fsp;
	}

	sconn->fsp_fi_cache.id = id;

	for (fsp=sconn->files;fsp;fsp=fsp->next) {
		if (file_id_equal(&fsp->file_id, &id)) {
			/* Setup positive cache. */
			sconn->fsp_fi_cache.fsp = fsp;
			return fsp;
		}
	}

	/* Setup negative cache. */
	sconn->fsp_fi_cache.fsp = NULL;
	return NULL;
}

/****************************************************************************
 Find the next fsp having the same device and inode.
****************************************************************************/

files_struct *file_find_di_next(files_struct *start_fsp)
{
	files_struct *fsp;

	for (fsp = start_fsp->next;fsp;fsp=fsp->next) {
		if (file_id_equal(&fsp->file_id, &start_fsp->file_id)) {
			return fsp;
		}
	}

	return NULL;
}

/****************************************************************************
 Find any fsp open with a pathname below that of an already open path.
****************************************************************************/

bool file_find_subpath(files_struct *dir_fsp)
{
	files_struct *fsp;
	size_t dlen;
	char *d_fullname = NULL;

	d_fullname = talloc_asprintf(talloc_tos(), "%s/%s",
				     dir_fsp->conn->connectpath,
				     dir_fsp->fsp_name->base_name);

	if (!d_fullname) {
		return false;
	}

	dlen = strlen(d_fullname);

	for (fsp=dir_fsp->conn->sconn->files; fsp; fsp=fsp->next) {
		char *d1_fullname;

		if (fsp == dir_fsp) {
			continue;
		}

		d1_fullname = talloc_asprintf(talloc_tos(),
					"%s/%s",
					fsp->conn->connectpath,
					fsp->fsp_name->base_name);

		/*
		 * If the open file has a path that is a longer
		 * component, then it's a subpath.
		 */
		if (strnequal(d_fullname, d1_fullname, dlen) &&
				(d1_fullname[dlen] == '/')) {
			TALLOC_FREE(d1_fullname);
			TALLOC_FREE(d_fullname);
			return true;
		}
		TALLOC_FREE(d1_fullname);
	}

	TALLOC_FREE(d_fullname);
	return false;
}

/****************************************************************************
 Sync open files on a connection.
****************************************************************************/

void file_sync_all(connection_struct *conn)
{
	files_struct *fsp, *next;

	for (fsp=conn->sconn->files; fsp; fsp=next) {
		next=fsp->next;
		if ((conn == fsp->conn) && (fsp->fh->fd != -1)) {
			sync_file(conn, fsp, True /* write through */);
		}
	}
}

/****************************************************************************
 Free up a fsp.
****************************************************************************/

void file_free(struct smb_request *req, files_struct *fsp)
{
	struct smbd_server_connection *sconn = fsp->conn->sconn;

	DLIST_REMOVE(sconn->files, fsp);

	TALLOC_FREE(fsp->fake_file_handle);

	if (fsp->fh->ref_count == 1) {
		TALLOC_FREE(fsp->fh);
	} else {
		fsp->fh->ref_count--;
	}

	if (fsp->notify) {
		if (fsp->is_directory) {
			notify_remove_onelevel(fsp->conn->notify_ctx,
					       &fsp->file_id, fsp);
		}
		notify_remove(fsp->conn->notify_ctx, fsp);
		TALLOC_FREE(fsp->notify);
	}

	/* Ensure this event will never fire. */
	TALLOC_FREE(fsp->oplock_timeout);

	/* Ensure this event will never fire. */
	TALLOC_FREE(fsp->update_write_time_event);

	bitmap_clear(sconn->file_bmap, fsp->fnum - FILE_HANDLE_OFFSET);
	sconn->files_used--;

	DEBUG(5,("freed files structure %d (%d used)\n",
		 fsp->fnum, sconn->files_used));

	fsp->conn->num_files_open--;

	if ((req != NULL) && (fsp == req->chain_fsp)) {
		req->chain_fsp = NULL;
	}

	/*
	 * Clear all possible chained fsp
	 * pointers in the SMB2 request queue.
	 */
	if (req != NULL && req->smb2req) {
		remove_smb2_chained_fsp(fsp);
	}

	/* Closing a file can invalidate the positive cache. */
	if (fsp == sconn->fsp_fi_cache.fsp) {
		ZERO_STRUCT(sconn->fsp_fi_cache);
	}

	/* Drop all remaining extensions. */
	while (fsp->vfs_extension) {
		vfs_remove_fsp_extension(fsp->vfs_extension->owner, fsp);
	}

	/* this is paranoia, just in case someone tries to reuse the
	   information */
	ZERO_STRUCTP(fsp);

	/* fsp->fsp_name is a talloc child and is free'd automatically. */
	TALLOC_FREE(fsp);
}

/****************************************************************************
 Get an fsp from a 16 bit fnum.
****************************************************************************/

static struct files_struct *file_fnum(struct smbd_server_connection *sconn,
				      uint16 fnum)
{
	files_struct *fsp;
	int count=0;

	for (fsp=sconn->files; fsp; fsp=fsp->next, count++) {
		if (fsp->fnum == fnum) {
			if (count > 10) {
				DLIST_PROMOTE(sconn->files, fsp);
			}
			return fsp;
		}
	}
	return NULL;
}

/****************************************************************************
 Get an fsp from a packet given a 16 bit fnum.
****************************************************************************/

files_struct *file_fsp(struct smb_request *req, uint16 fid)
{
	files_struct *fsp;

	if (req == NULL) {
		/*
		 * We should never get here. req==NULL could in theory
		 * only happen from internal opens with a non-zero
		 * root_dir_fid. Internal opens just don't do that, at
		 * least they are not supposed to do so. And if they
		 * start to do so, they better fake up a smb_request
		 * from which we get the right smbd_server_conn. While
		 * this should never happen, let's return NULL here.
		 */
		return NULL;
	}

	if (req->chain_fsp != NULL) {
		return req->chain_fsp;
	}

	fsp = file_fnum(req->sconn, fid);
	if (fsp != NULL) {
		req->chain_fsp = fsp;
	}
	return fsp;
}

uint64_t fsp_persistent_id(const struct files_struct *fsp)
{
	uint64_t persistent_id;

	/*
	 * This calculates a number that is most likely
	 * globally unique. In future we will have a database
	 * to make it completely unique.
	 *
	 * 32-bit random gen_id
	 * 16-bit truncated open_time
	 * 16-bit fnum (valatile_id)
	 */
	persistent_id = fsp->fh->gen_id & UINT32_MAX;
	persistent_id <<= 16;
	persistent_id &= 0x0000FFFFFFFF0000LLU;
	persistent_id |= fsp->open_time.tv_usec & UINT16_MAX;
	persistent_id <<= 16;
	persistent_id &= 0xFFFFFFFFFFFF0000LLU;
	persistent_id |= fsp->fnum & UINT16_MAX;

	return persistent_id;
}

struct files_struct *file_fsp_smb2(struct smbd_smb2_request *smb2req,
				   uint64_t persistent_id,
				   uint64_t volatile_id)
{
	struct files_struct *fsp;
	uint64_t fsp_persistent;

	if (smb2req->compat_chain_fsp != NULL) {
		return smb2req->compat_chain_fsp;
	}

	if (volatile_id > UINT16_MAX) {
		return NULL;
	}

	fsp = file_fnum(smb2req->sconn, (uint16_t)volatile_id);
	if (fsp == NULL) {
		return NULL;
	}
	fsp_persistent = fsp_persistent_id(fsp);

	if (persistent_id != fsp_persistent) {
		return NULL;
	}

	if (smb2req->tcon == NULL) {
		return NULL;
	}

	if (smb2req->tcon->compat_conn != fsp->conn) {
		return NULL;
	}

	if (smb2req->session == NULL) {
		return NULL;
	}

	if (smb2req->session->vuid != fsp->vuid) {
		return NULL;
	}

	smb2req->compat_chain_fsp = fsp;
	return fsp;
}

/****************************************************************************
 Duplicate the file handle part for a DOS or FCB open.
****************************************************************************/

NTSTATUS dup_file_fsp(struct smb_request *req, files_struct *from,
		      uint32 access_mask, uint32 share_access,
		      uint32 create_options, files_struct *to)
{
	TALLOC_FREE(to->fh);

	to->fh = from->fh;
	to->fh->ref_count++;

	to->file_id = from->file_id;
	to->initial_allocation_size = from->initial_allocation_size;
	to->mode = from->mode;
	to->file_pid = from->file_pid;
	to->vuid = from->vuid;
	to->open_time = from->open_time;
	to->access_mask = access_mask;
	to->share_access = share_access;
	to->oplock_type = from->oplock_type;
	to->can_lock = from->can_lock;
	to->can_read = (access_mask & (FILE_READ_DATA)) ? True : False;
	if (!CAN_WRITE(from->conn)) {
		to->can_write = False;
	} else {
		to->can_write = (access_mask & (FILE_WRITE_DATA | FILE_APPEND_DATA)) ? True : False;
	}
	to->modified = from->modified;
	to->is_directory = from->is_directory;
	to->aio_write_behind = from->aio_write_behind;

	if (from->print_file) {
		to->print_file = talloc(to, struct print_file_data);
		if (!to->print_file) return NT_STATUS_NO_MEMORY;
		to->print_file->rap_jobid = from->print_file->rap_jobid;
	} else {
		to->print_file = NULL;
	}

	return fsp_set_smb_fname(to, from->fsp_name);
}

/**
 * Return a jenkins hash of a pathname on a connection.
 */

NTSTATUS file_name_hash(connection_struct *conn,
			const char *name, uint32_t *p_name_hash)
{
	TDB_DATA key;
	char *fullpath = NULL;

	/* Set the hash of the full pathname. */
	fullpath = talloc_asprintf(talloc_tos(),
			"%s/%s",
			conn->connectpath,
			name);
	if (!fullpath) {
		return NT_STATUS_NO_MEMORY;
	}
	key = string_term_tdb_data(fullpath);
	*p_name_hash = tdb_jenkins_hash(&key);

	DEBUG(10,("file_name_hash: %s hash 0x%x\n",
		fullpath,
		(unsigned int)*p_name_hash ));

	TALLOC_FREE(fullpath);
	return NT_STATUS_OK;
}

/**
 * The only way that the fsp->fsp_name field should ever be set.
 */
NTSTATUS fsp_set_smb_fname(struct files_struct *fsp,
			   const struct smb_filename *smb_fname_in)
{
	NTSTATUS status;
	struct smb_filename *smb_fname_new;

	status = copy_smb_filename(fsp, smb_fname_in, &smb_fname_new);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	TALLOC_FREE(fsp->fsp_name);
	fsp->fsp_name = smb_fname_new;

	return file_name_hash(fsp->conn,
			smb_fname_str_dbg(fsp->fsp_name),
			&fsp->name_hash);
}
