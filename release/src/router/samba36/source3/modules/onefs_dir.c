/*
 * Unix SMB/CIFS implementation.
 *
 * Support for OneFS bulk directory enumeration API
 *
 * Copyright (C) Steven Danneman, 2009
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
#include "smbd/smbd.h"
#include "onefs.h"
#include "onefs_config.h"

#include <ifs/ifs_syscalls.h>
#include <isi_util/isi_dir.h>

/* The OneFS filesystem provides a readdirplus() syscall, equivalent to the
 * NFSv3 PDU, which retrieves bulk directory listings with stat information
 * in a single syscall.
 *
 * This file hides this bulk interface underneath Samba's very POSIX like
 * opendir/readdir/telldir VFS interface.  This is done to provide a
 * significant performance improvement when listing the contents of large
 * directories, which also require file meta information. ie a typical
 * Windows Explorer request.
 */

#define RDP_RESUME_KEY_START 0x1

#define RDP_BATCH_SIZE 128
#define RDP_DIRENTRIES_SIZE ((size_t)(RDP_BATCH_SIZE * sizeof(struct dirent)))

static char *rdp_direntries = NULL;
static struct stat *rdp_stats = NULL;
static uint64_t *rdp_cookies = NULL;

struct rdp_dir_state {
	struct rdp_dir_state *next, *prev;
	SMB_STRUCT_DIR *dirp;
	char *direntries_cursor; /* cursor to last returned direntry in cache */
	size_t stat_count;	 /* number of entries stored in the cache */
	size_t stat_cursor;	 /* cursor to last returned stat in the cache */
	uint64_t resume_cookie;  /* cookie from the last entry returned from the
				    cache */
};

static struct rdp_dir_state *dirstatelist = NULL;

SMB_STRUCT_DIR *rdp_last_dirp = NULL;

/**
 * Given a DIR pointer, return our internal state.
 *
 * This function also tells us whether the given DIR is the same as we saw
 * during the last call.  Because we use a single globally allocated buffer
 * for readdirplus entries we must check every call into this API to see if
 * it's for the same directory listing, or a new one. If it's the same we can
 * maintain our current cached entries, otherwise we must go to the kernel.
 *
 * @return 0 on success, 1 on failure
 */
static int
rdp_retrieve_dir_state(SMB_STRUCT_DIR *dirp, struct rdp_dir_state **dir_state,
		       bool *same_as_last)
{
	struct rdp_dir_state *dsp;

	/* Is this directory the same as the last call */
	*same_as_last = (dirp == rdp_last_dirp);

	for(dsp = dirstatelist; dsp; dsp = dsp->next)
		if (dsp->dirp == dirp) {
			*dir_state = dsp;
			return 0;
		}

	/* Couldn't find existing dir_state for the given directory
	 * pointer. */
	return 1;
}

/**
 * Initialize the global readdirplus buffers.
 *
 * These same buffers are used for all calls into readdirplus.
 *
 * @return 0 on success, errno value on failure
 */
static int
rdp_init(struct rdp_dir_state *dsp)
{
	/* Unfortunately, there is no good way to free these buffers.  If we
	 * allocated and freed for every DIR handle performance would be
	 * adversely affected.  For now these buffers will be leaked and only
	 * freed when the smbd process dies. */
	if (!rdp_direntries) {
		rdp_direntries = SMB_MALLOC(RDP_DIRENTRIES_SIZE);
		if (!rdp_direntries)
			return ENOMEM;
	}

	if (!rdp_stats) {
		rdp_stats =
		    SMB_MALLOC(RDP_BATCH_SIZE * sizeof(struct stat));
		if (!rdp_stats)
			return ENOMEM;
	}

	if (!rdp_cookies) {
		rdp_cookies = SMB_MALLOC(RDP_BATCH_SIZE * sizeof(uint64_t));
		if (!rdp_cookies)
			return ENOMEM;
	}

	dsp->direntries_cursor = rdp_direntries + RDP_DIRENTRIES_SIZE;
	dsp->stat_count = RDP_BATCH_SIZE;
	dsp->stat_cursor = RDP_BATCH_SIZE;
	dsp->resume_cookie = RDP_RESUME_KEY_START;

	return 0;
}

/**
 * Call into readdirplus() to refill our global dirent cache.
 *
 * This function also resets all cursors back to the beginning of the cache.
 * All stat buffers are retrieved by following symlinks.
 *
 * @return number of entries retrieved, -1 on error
 */
static int
rdp_fill_cache(struct rdp_dir_state *dsp)
{
	int nread, dirfd;

	dirfd = dirfd(dsp->dirp);
	if (dirfd < 0) {
		DEBUG(1, ("Could not retrieve fd for DIR\n"));
		return -1;
	}

	/* Resize the stat_count to grab as many entries as possible */
	dsp->stat_count = RDP_BATCH_SIZE;

	DEBUG(9, ("Calling readdirplus() with DIR %p, dirfd: %d, "
		 "resume_cookie %#llx, size_to_read: %zu, "
		 "direntries_size: %zu, stat_count: %u\n",
		 dsp->dirp, dirfd, dsp->resume_cookie, RDP_BATCH_SIZE,
		 RDP_DIRENTRIES_SIZE, dsp->stat_count));

	nread = readdirplus(dirfd,
			    RDP_FOLLOW,
			    &dsp->resume_cookie,
			    RDP_BATCH_SIZE,
			    rdp_direntries,
			    RDP_DIRENTRIES_SIZE,
			    &dsp->stat_count,
			    rdp_stats,
			    rdp_cookies);
	if (nread < 0) {
		DEBUG(1, ("Error calling readdirplus(): %s\n",
			 strerror(errno)));
		return -1;
	}

	DEBUG(9, ("readdirplus() returned %u entries from DIR %p\n",
		 dsp->stat_count, dsp->dirp));

	dsp->direntries_cursor = rdp_direntries;
	dsp->stat_cursor = 0;

	return nread;
}

/**
 * Create a dir_state to track an open directory that we're enumerating.
 *
 * This utility function is globally accessible for use by other parts of the
 * onefs.so module to initialize a dir_state when a directory is opened through
 * a path other than the VFS layer.
 *
 * @return 0 on success and errno on failure
 *
 * @note: Callers of this function MUST cleanup the dir_state through a proper
 * call to VFS_CLOSEDIR().
 */
int
onefs_rdp_add_dir_state(connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
	int ret = 0;
	struct rdp_dir_state *dsp = NULL;

	/* No-op if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return 0;
	}

	/* Create a struct dir_state */
	dsp = SMB_MALLOC_P(struct rdp_dir_state);
	if (!dsp) {
		DEBUG(0, ("Error allocating struct rdp_dir_state.\n"));
		return ENOMEM;
	}

	/* Initialize the dir_state structure and add it to the list */
	ret = rdp_init(dsp);
	if (ret) {
		DEBUG(0, ("Error initializing readdirplus() buffers: %s\n",
			 strerror(ret)));
		return ret;
	}

	/* Set the SMB_STRUCT_DIR in the dsp */
	dsp->dirp = dirp;

	DLIST_ADD(dirstatelist, dsp);

	return 0;
}

/**
 * Open a directory for enumeration.
 *
 * Create a state struct to track the state of this directory for the life
 * of this open.
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] fname filename of the directory to open
 * @param[in] mask unused
 * @param[in] attr unused
 *
 * @return DIR pointer, NULL if directory does not exist, NULL on error
 */
SMB_STRUCT_DIR *
onefs_opendir(vfs_handle_struct *handle, const char *fname, const char *mask,
	      uint32 attr)
{
	int ret = 0;
	SMB_STRUCT_DIR *ret_dirp;

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
	}

	/* Open the directory */
	ret_dirp = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
	if (!ret_dirp) {
		DEBUG(3, ("Unable to open directory: %s\n", fname));
		return NULL;
	}

	/* Create the dir_state struct and add it to the list */
	ret = onefs_rdp_add_dir_state(handle->conn, ret_dirp);
	if (ret) {
		DEBUG(0, ("Error adding dir_state to the list\n"));
		return NULL;
	}

	DEBUG(9, ("Opened handle on directory: \"%s\", DIR %p\n",
		 fname, ret_dirp));

	return ret_dirp;
}

/**
 * Retrieve one direntry and optional stat buffer from our readdir cache.
 *
 * Increment the internal resume cookie, and refresh the cache from the
 * kernel if necessary.
 *
 * The cache cursor tracks the last entry which was successfully returned
 * to a caller of onefs_readdir().  When a new entry is requested, this
 * function first increments the cursor, then returns that entry.
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle to retrieve direntries from
 * @param[in/out] sbuf optional stat buffer to fill, this can be NULL
 *
 * @return dirent structure, NULL if at the end of the directory, NULL on error
 */
SMB_STRUCT_DIRENT *
onefs_readdir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp,
	      SMB_STRUCT_STAT *sbuf)
{
	struct rdp_dir_state *dsp = NULL;
	SMB_STRUCT_DIRENT *ret_direntp;
	bool same_as_last, filled_cache = false;
	int ret = -1;

	/* Set stat invalid in-case we error out */
	if (sbuf)
		SET_STAT_INVALID(*sbuf);

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return sys_readdir(dirp);
	}

	/* Retrieve state based off DIR handle */
	ret = rdp_retrieve_dir_state(dirp, &dsp, &same_as_last);
	if (ret) {
		DEBUG(1, ("Could not retrieve dir_state struct for "
			 "SMB_STRUCT_DIR pointer.\n"));
		ret_direntp = NULL;
		goto end;
	}

	/* DIR is the same, current buffer and cursors are valid.
	 * Check if there are any entries left in our current cache. */
	if (same_as_last) {
		if (dsp->stat_cursor == dsp->stat_count - 1) {
			/* Cache is empty, refill from kernel */
			ret = rdp_fill_cache(dsp);
			if (ret <= 0) {
				ret_direntp = NULL;
				goto end;
			}
			filled_cache = true;
		}
	} else {
		/* DIR is different from last call, reset all buffers and
		 * cursors, and refill the global cache from the new DIR */
		ret = rdp_fill_cache(dsp);
		if (ret <= 0) {
			ret_direntp = NULL;
			goto end;
		}
		filled_cache = true;
		DEBUG(8, ("Switched global rdp cache to new DIR entry.\n"));
	}

	/* If we just filled the cache we treat that action as the cursor
	 * increment as the resume cookie used belonged to the previous
	 * directory entry.  If the cache has not changed we first increment
	 * our cursor, then return the next entry */
	if (!filled_cache) {
		dsp->direntries_cursor +=
		    ((SMB_STRUCT_DIRENT *)dsp->direntries_cursor)->d_reclen;
		dsp->stat_cursor++;
	}

	/* The resume_cookie stored here purposely differs based on whether we
	 * just filled the cache. The resume cookie stored must always provide
	 * the next direntry, in case the cache is reloaded on every
	 * onefs_readdir() */
	dsp->resume_cookie = rdp_cookies[dsp->stat_cursor];

	/* Return an entry from cache */
	ret_direntp = ((SMB_STRUCT_DIRENT *)dsp->direntries_cursor);
	if (sbuf) {
		struct stat onefs_sbuf;

		onefs_sbuf = rdp_stats[dsp->stat_cursor];
		init_stat_ex_from_onefs_stat(sbuf, &onefs_sbuf);

		/* readdirplus() sets st_ino field to 0, if it was
		 * unable to retrieve stat information for that
		 * particular directory entry. */
		if (sbuf->st_ex_ino == 0)
			SET_STAT_INVALID(*sbuf);
	}

	DEBUG(9, ("Read from DIR %p, direntry: \"%s\", resume cookie: %#llx, "
		 "cache cursor: %zu, cache count: %zu\n",
		 dsp->dirp, ret_direntp->d_name, dsp->resume_cookie,
		 dsp->stat_cursor, dsp->stat_count));

	/* FALLTHROUGH */
end:
	/* Set rdp_last_dirp at the end of every VFS call where the cache was
	 * reloaded */
	rdp_last_dirp = dirp;
	return ret_direntp;
}

/**
 * Set the location of the next direntry to be read via onefs_readdir().
 *
 * This function should only pass in locations retrieved from onefs_telldir().
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle to set offset on
 * @param[in] offset into the directory to resume reading from
 *
 * @return no return value
 */
void
onefs_seekdir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp, long offset)
{
	struct rdp_dir_state *dsp = NULL;
	bool same_as_last;
	uint64_t resume_cookie = 0;
	int ret = -1;

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return sys_seekdir(dirp, offset);
	}

	/* Validate inputs */
	if (offset < 0) {
		DEBUG(1, ("Invalid offset %ld passed.\n", offset));
		return;
	}

	/* Retrieve state based off DIR handle */
	ret = rdp_retrieve_dir_state(dirp, &dsp, &same_as_last);
	if (ret) {
		DEBUG(1, ("Could not retrieve dir_state struct for "
			 "SMB_STRUCT_DIR pointer.\n"));
		/* XXX: we can't return an error, should we ABORT rather than
		 * return without actually seeking? */
		return;
	}

	/* Convert offset to resume_cookie */
	resume_cookie = rdp_offset31_to_cookie63(offset);

	DEBUG(9, ("Seek DIR %p, offset: %ld, resume_cookie: %#llx\n",
		 dsp->dirp, offset, resume_cookie));

	/* TODO: We could check if the resume_cookie is already in the cache
	 * through a linear search.  This would allow us to avoid the cost of
	 * flushing the cache.  Frequently, the seekdir offset will only be
	 * one entry before the current cache cursor.  However, usually
	 * VFS_SEEKDIR() is only called at the end of a TRAND2_FIND read and
	 * we'll flush the cache at the beginning of the next PDU anyway. Some
	 * analysis should be done to see if this enhancement would provide
	 * better performance. */

	/* Set the resume cookie and indicate that the cache should be reloaded
	 * on next read */
	dsp->resume_cookie = resume_cookie;
	rdp_last_dirp = NULL;

	return;
}

/**
 * Returns the location of the next direntry to be read via onefs_readdir().
 *
 * This value can be passed into onefs_seekdir().
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle to set offset on
 *
 * @return offset into the directory to resume reading from
 */
long
onefs_telldir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	struct rdp_dir_state *dsp = NULL;
	bool same_as_last;
	long offset;
	int ret = -1;

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return sys_telldir(dirp);
	}

	/* Retrieve state based off DIR handle */
	ret = rdp_retrieve_dir_state(dirp, &dsp, &same_as_last);
	if (ret) {
		DEBUG(1, ("Could not retrieve dir_state struct for "
			 "SMB_STRUCT_DIR pointer.\n"));
		return -1;
	}

	/* Convert resume_cookie to offset */
	offset = rdp_cookie63_to_offset31(dsp->resume_cookie);
	if (offset < 0) {
		DEBUG(1, ("Unable to convert resume_cookie: %#llx to a "
			 "suitable 32-bit offset value. Error: %s\n",
			 dsp->resume_cookie, strerror(errno)));
		return -1;
	}

	DEBUG(9, ("Seek DIR %p, offset: %ld, resume_cookie: %#llx\n",
		 dsp->dirp, offset, dsp->resume_cookie));

	return offset;
}

/**
 * Set the next direntry to be read via onefs_readdir() to the beginning of the
 * directory.
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle to set offset on
 *
 * @return no return value
 */
void
onefs_rewinddir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	struct rdp_dir_state *dsp = NULL;
	bool same_as_last;
	int ret = -1;

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return sys_rewinddir(dirp);
	}

	/* Retrieve state based off DIR handle */
	ret = rdp_retrieve_dir_state(dirp, &dsp, &same_as_last);
	if (ret) {
		DEBUG(1, ("Could not retrieve dir_state struct for "
			 "SMB_STRUCT_DIR pointer.\n"));
		return;
	}

	/* Reset location and resume key to beginning */
	ret = rdp_init(dsp);
	if (ret) {
		DEBUG(0, ("Error re-initializing rdp cursors: %s\n",
		    strerror(ret)));
		return;
	}

	DEBUG(9, ("Rewind DIR: %p, to resume_cookie: %#llx\n", dsp->dirp,
		 dsp->resume_cookie));

	return;
}

/**
 * Close DIR pointer and remove all state for that directory open.
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle to set offset on
 *
 * @return -1 on failure, setting errno
 */
int
onefs_closedir(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	struct rdp_dir_state *dsp = NULL;
	bool same_as_last;
	int ret_val = -1;
	int ret = -1;

	/* Fallback to default system routines if readdirplus is disabled */
	if (!lp_parm_bool(SNUM(handle->conn), PARM_ONEFS_TYPE,
	    PARM_USE_READDIRPLUS, PARM_USE_READDIRPLUS_DEFAULT))
	{
		return SMB_VFS_NEXT_CLOSEDIR(handle, dirp);
	}

	/* Retrieve state based off DIR handle */
	ret = rdp_retrieve_dir_state(dirp, &dsp, &same_as_last);
	if (ret) {
		DEBUG(1, ("Could not retrieve dir_state struct for "
			 "SMB_STRUCT_DIR pointer.\n"));
		errno = ENOENT;
		return -1;
	}

	/* Close DIR pointer */
	ret_val = SMB_VFS_NEXT_CLOSEDIR(handle, dsp->dirp);

	DEBUG(9, ("Closed handle on DIR %p\n", dsp->dirp));

	/* Tear down state struct */
	DLIST_REMOVE(dirstatelist, dsp);
	SAFE_FREE(dsp);

	/* Set lastp to NULL, as cache is no longer valid */
	rdp_last_dirp = NULL;

	return ret_val;
}

/**
 * Initialize cache data at the beginning of every SMB search operation
 *
 * Since filesystem operations, such as delete files or meta data
 * updates can occur to files in the directory we're searching
 * between FIND_FIRST and FIND_NEXT calls we must refresh the cache
 * from the kernel on every new search SMB.
 *
 * @param[in] handle vfs handle given in most VFS calls
 * @param[in] dirp system DIR handle for the current search
 *
 * @return nothing
 */
void
onefs_init_search_op(vfs_handle_struct *handle,  SMB_STRUCT_DIR *dirp)
{
	/* Setting the rdp_last_dirp to NULL will cause the next readdir
	 * operation to refill the cache. */
	rdp_last_dirp = NULL;

	return;
}
