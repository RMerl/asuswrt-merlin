/*
 * Recycle bin VFS module for Samba.
 *
 * Copyright (C) 2001, Brandon Stone, Amherst College, <bbstone@amherst.edu>.
 * Copyright (C) 2002, Jeremy Allison - modified to make a VFS module.
 * Copyright (C) 2002, Alexander Bokovoy - cascaded VFS adoption,
 * Copyright (C) 2002, Juergen Hasch - added some options.
 * Copyright (C) 2002, Simo Sorce
 * Copyright (C) 2002, Stefan (metze) Metzmacher
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
#include "system/filesys.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "auth.h"

#define ALLOC_CHECK(ptr, label) do { if ((ptr) == NULL) { DEBUG(0, ("recycle.bin: out of memory!\n")); errno = ENOMEM; goto label; } } while(0)

static int vfs_recycle_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_recycle_debug_level
 
static int recycle_connect(vfs_handle_struct *handle, const char *service, const char *user);
static void recycle_disconnect(vfs_handle_struct *handle);
static int recycle_unlink(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname);

static int recycle_connect(vfs_handle_struct *handle, const char *service, const char *user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	DEBUG(10,("recycle_connect() connect to service[%s] as user[%s].\n",
		service,user));

	return 0;
}

static void recycle_disconnect(vfs_handle_struct *handle)
{
	DEBUG(10,("recycle_disconnect() connect to service[%s].\n",
		lp_servicename(SNUM(handle->conn))));

	SMB_VFS_NEXT_DISCONNECT(handle);
}

static const char *recycle_repository(vfs_handle_struct *handle)
{
	const char *tmp_str = NULL;

	tmp_str = lp_parm_const_string(SNUM(handle->conn), "recycle", "repository",".recycle");

	DEBUG(10, ("recycle: repository = %s\n", tmp_str));

	return tmp_str;
}

static bool recycle_keep_dir_tree(vfs_handle_struct *handle)
{
	bool ret;

	ret = lp_parm_bool(SNUM(handle->conn), "recycle", "keeptree", False);

	DEBUG(10, ("recycle_bin: keeptree = %s\n", ret?"True":"False"));

	return ret;
}

static bool recycle_versions(vfs_handle_struct *handle)
{
	bool ret;

	ret = lp_parm_bool(SNUM(handle->conn), "recycle", "versions", False);

	DEBUG(10, ("recycle: versions = %s\n", ret?"True":"False"));

	return ret;
}

static bool recycle_touch(vfs_handle_struct *handle)
{
	bool ret;

	ret = lp_parm_bool(SNUM(handle->conn), "recycle", "touch", False);

	DEBUG(10, ("recycle: touch = %s\n", ret?"True":"False"));

	return ret;
}

static bool recycle_touch_mtime(vfs_handle_struct *handle)
{
	bool ret;

	ret = lp_parm_bool(SNUM(handle->conn), "recycle", "touch_mtime", False);

	DEBUG(10, ("recycle: touch_mtime = %s\n", ret?"True":"False"));

	return ret;
}

static const char **recycle_exclude(vfs_handle_struct *handle)
{
	const char **tmp_lp;

	tmp_lp = lp_parm_string_list(SNUM(handle->conn), "recycle", "exclude", NULL);

	DEBUG(10, ("recycle: exclude = %s ...\n", tmp_lp?*tmp_lp:""));

	return tmp_lp;
}

static const char **recycle_exclude_dir(vfs_handle_struct *handle)
{
	const char **tmp_lp;

	tmp_lp = lp_parm_string_list(SNUM(handle->conn), "recycle", "exclude_dir", NULL);

	DEBUG(10, ("recycle: exclude_dir = %s ...\n", tmp_lp?*tmp_lp:""));

	return tmp_lp;
}

static const char **recycle_noversions(vfs_handle_struct *handle)
{
	const char **tmp_lp;

	tmp_lp = lp_parm_string_list(SNUM(handle->conn), "recycle", "noversions", NULL);

	DEBUG(10, ("recycle: noversions = %s\n", tmp_lp?*tmp_lp:""));

	return tmp_lp;
}

static SMB_OFF_T recycle_maxsize(vfs_handle_struct *handle)
{
	SMB_OFF_T maxsize;

	maxsize = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
					    "recycle", "maxsize", NULL));

	DEBUG(10, ("recycle: maxsize = %lu\n", (long unsigned int)maxsize));

	return maxsize;
}

static SMB_OFF_T recycle_minsize(vfs_handle_struct *handle)
{
	SMB_OFF_T minsize;

	minsize = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
					    "recycle", "minsize", NULL));

	DEBUG(10, ("recycle: minsize = %lu\n", (long unsigned int)minsize));

	return minsize;
}

static mode_t recycle_directory_mode(vfs_handle_struct *handle)
{
	int dirmode;
	const char *buff;

	buff = lp_parm_const_string(SNUM(handle->conn), "recycle", "directory_mode", NULL);

	if (buff != NULL ) {
		sscanf(buff, "%o", &dirmode);
	} else {
		dirmode=S_IRUSR | S_IWUSR | S_IXUSR;
	}

	DEBUG(10, ("recycle: directory_mode = %o\n", dirmode));
	return (mode_t)dirmode;
}

static mode_t recycle_subdir_mode(vfs_handle_struct *handle)
{
	int dirmode;
	const char *buff;

	buff = lp_parm_const_string(SNUM(handle->conn), "recycle", "subdir_mode", NULL);

	if (buff != NULL ) {
		sscanf(buff, "%o", &dirmode);
	} else {
		dirmode=recycle_directory_mode(handle);
	}

	DEBUG(10, ("recycle: subdir_mode = %o\n", dirmode));
	return (mode_t)dirmode;
}

static bool recycle_directory_exist(vfs_handle_struct *handle, const char *dname)
{
	SMB_STRUCT_STAT st;

	if (vfs_stat_smb_fname(handle->conn, dname, &st) == 0) {
		if (S_ISDIR(st.st_ex_mode)) {
			return True;
		}
	}

	return False;
}

static bool recycle_file_exist(vfs_handle_struct *handle,
			       const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	NTSTATUS status;
	bool ret = false;

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	if (SMB_VFS_STAT(handle->conn, smb_fname_tmp) == 0) {
		if (S_ISREG(smb_fname_tmp->st.st_ex_mode)) {
			ret = true;
		}
	}

	TALLOC_FREE(smb_fname_tmp);
	return ret;
}

/**
 * Return file size
 * @param conn connection
 * @param fname file name
 * @return size in bytes
 **/
static SMB_OFF_T recycle_get_file_size(vfs_handle_struct *handle,
				       const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	NTSTATUS status;
	SMB_OFF_T size;

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		size = (SMB_OFF_T)0;
		goto out;
	}

	if (SMB_VFS_STAT(handle->conn, smb_fname_tmp) != 0) {
		DEBUG(0,("recycle: stat for %s returned %s\n",
			 smb_fname_str_dbg(smb_fname_tmp), strerror(errno)));
		size = (SMB_OFF_T)0;
		goto out;
	}

	size = smb_fname_tmp->st.st_ex_size;
 out:
	TALLOC_FREE(smb_fname_tmp);
	return size;
}

/**
 * Create directory tree
 * @param conn connection
 * @param dname Directory tree to be created
 * @return Returns True for success
 **/
static bool recycle_create_dir(vfs_handle_struct *handle, const char *dname)
{
	size_t len;
	mode_t mode;
	char *new_dir = NULL;
	char *tmp_str = NULL;
	char *token;
	char *tok_str;
	bool ret = False;
	char *saveptr;

	mode = recycle_directory_mode(handle);

	tmp_str = SMB_STRDUP(dname);
	ALLOC_CHECK(tmp_str, done);
	tok_str = tmp_str;

	len = strlen(dname)+1;
	new_dir = (char *)SMB_MALLOC(len + 1);
	ALLOC_CHECK(new_dir, done);
	*new_dir = '\0';
	if (dname[0] == '/') {
		/* Absolute path. */
		safe_strcat(new_dir,"/",len);
	}

	/* Create directory tree if neccessary */
	for(token = strtok_r(tok_str, "/", &saveptr); token;
	    token = strtok_r(NULL, "/", &saveptr)) {
		safe_strcat(new_dir, token, len);
		if (recycle_directory_exist(handle, new_dir))
			DEBUG(10, ("recycle: dir %s already exists\n", new_dir));
		else {
			DEBUG(5, ("recycle: creating new dir %s\n", new_dir));
			if (SMB_VFS_NEXT_MKDIR(handle, new_dir, mode) != 0) {
				DEBUG(1,("recycle: mkdir failed for %s with error: %s\n", new_dir, strerror(errno)));
				ret = False;
				goto done;
			}
		}
		safe_strcat(new_dir, "/", len);
		mode = recycle_subdir_mode(handle);
	}

	ret = True;
done:
	SAFE_FREE(tmp_str);
	SAFE_FREE(new_dir);
	return ret;
}

/**
 * Check if any of the components of "exclude_list" are contained in path.
 * Return True if found
 **/

static bool matchdirparam(const char **dir_exclude_list, char *path)
{
	char *startp = NULL, *endp = NULL;

	if (dir_exclude_list == NULL || dir_exclude_list[0] == NULL ||
		*dir_exclude_list[0] == '\0' || path == NULL || *path == '\0') {
		return False;
	}

	/* 
	 * Walk the components of path, looking for matches with the
	 * exclude list on each component. 
	 */

	for (startp = path; startp; startp = endp) {
		int i;

		while (*startp == '/') {
			startp++;
		}
		endp = strchr(startp, '/');
		if (endp) {
			*endp = '\0';
		}

		for(i=0; dir_exclude_list[i] ; i++) {
			if(unix_wild_match(dir_exclude_list[i], startp)) {
				/* Repair path. */
				if (endp) {
					*endp = '/';
				}
				return True;
			}
		}

		/* Repair path. */
		if (endp) {
			*endp = '/';
		}
	}

	return False;
}

/**
 * Check if needle is contained in haystack, * and ? patterns are resolved
 * @param haystack list of parameters separated by delimimiter character
 * @param needle string to be matched exectly to haystack including pattern matching
 * @return True if found
 **/
static bool matchparam(const char **haystack_list, const char *needle)
{
	int i;

	if (haystack_list == NULL || haystack_list[0] == NULL ||
		*haystack_list[0] == '\0' || needle == NULL || *needle == '\0') {
		return False;
	}

	for(i=0; haystack_list[i] ; i++) {
		if(unix_wild_match(haystack_list[i], needle)) {
			return True;
		}
	}

	return False;
}

/**
 * Touch access or modify date
 **/
static void recycle_do_touch(vfs_handle_struct *handle,
			     const struct smb_filename *smb_fname,
			     bool touch_mtime)
{
	struct smb_filename *smb_fname_tmp = NULL;
	struct smb_file_time ft;
	NTSTATUS status;
	int ret, err;

	ZERO_STRUCT(ft);

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

	if (SMB_VFS_STAT(handle->conn, smb_fname_tmp) != 0) {
		DEBUG(0,("recycle: stat for %s returned %s\n",
			 smb_fname_str_dbg(smb_fname_tmp), strerror(errno)));
		goto out;
	}
	/* atime */
	ft.atime = timespec_current();
	/* mtime */
	ft.mtime = touch_mtime ? ft.atime : smb_fname_tmp->st.st_ex_mtime;

	become_root();
	ret = SMB_VFS_NEXT_NTIMES(handle, smb_fname_tmp, &ft);
	err = errno;
	unbecome_root();
	if (ret == -1 ) {
		DEBUG(0, ("recycle: touching %s failed, reason = %s\n",
			  smb_fname_str_dbg(smb_fname_tmp), strerror(err)));
	}
 out:
	TALLOC_FREE(smb_fname_tmp);
}

/**
 * Check if file should be recycled
 **/
static int recycle_unlink(vfs_handle_struct *handle,
    const struct smb_filename *smb_fname)
{
	connection_struct *conn = handle->conn;
	char *path_name = NULL;
       	char *temp_name = NULL;
	char *final_name = NULL;
	struct smb_filename *smb_fname_final = NULL;
	const char *base;
	char *repository = NULL;
	int i = 1;
	SMB_OFF_T maxsize, minsize;
	SMB_OFF_T file_size; /* space_avail;	*/
	bool exist;
	NTSTATUS status;
	int rc = -1;

	repository = talloc_sub_advanced(NULL, lp_servicename(SNUM(conn)),
					conn->session_info->unix_name,
					conn->connectpath,
					conn->session_info->utok.gid,
					conn->session_info->sanitized_username,
					conn->session_info->info3->base.domain.string,
					recycle_repository(handle));
	ALLOC_CHECK(repository, done);
	/* shouldn't we allow absolute path names here? --metze */
	/* Yes :-). JRA. */
	trim_char(repository, '\0', '/');

	if(!repository || *(repository) == '\0') {
		DEBUG(3, ("recycle: repository path not set, purging %s...\n",
			  smb_fname_str_dbg(smb_fname)));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	/* we don't recycle the recycle bin... */
	if (strncmp(smb_fname->base_name, repository,
		    strlen(repository)) == 0) {
		DEBUG(3, ("recycle: File is within recycling bin, unlinking ...\n"));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	file_size = recycle_get_file_size(handle, smb_fname);
	/* it is wrong to purge filenames only because they are empty imho
	 *   --- simo
	 *
	if(fsize == 0) {
		DEBUG(3, ("recycle: File %s is empty, purging...\n", file_name));
		rc = SMB_VFS_NEXT_UNLINK(handle,file_name);
		goto done;
	}
	 */

	/* FIXME: this is wrong, we should check the whole size of the recycle bin is
	 * not greater then maxsize, not the size of the single file, also it is better
	 * to remove older files
	 */
	maxsize = recycle_maxsize(handle);
	if(maxsize > 0 && file_size > maxsize) {
		DEBUG(3, ("recycle: File %s exceeds maximum recycle size, "
			  "purging... \n", smb_fname_str_dbg(smb_fname)));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}
	minsize = recycle_minsize(handle);
	if(minsize > 0 && file_size < minsize) {
		DEBUG(3, ("recycle: File %s lowers minimum recycle size, "
			  "purging... \n", smb_fname_str_dbg(smb_fname)));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	/* FIXME: this is wrong: moving files with rename does not change the disk space
	 * allocation
	 *
	space_avail = SMB_VFS_NEXT_DISK_FREE(handle, ".", True, &bsize, &dfree, &dsize) * 1024L;
	DEBUG(5, ("space_avail = %Lu, file_size = %Lu\n", space_avail, file_size));
	if(space_avail < file_size) {
		DEBUG(3, ("recycle: Not enough diskspace, purging file %s\n", file_name));
		rc = SMB_VFS_NEXT_UNLINK(handle, file_name);
		goto done;
	}
	 */

	/* extract filename and path */
	base = strrchr(smb_fname->base_name, '/');
	if (base == NULL) {
		base = smb_fname->base_name;
		path_name = SMB_STRDUP("/");
		ALLOC_CHECK(path_name, done);
	}
	else {
		path_name = SMB_STRDUP(smb_fname->base_name);
		ALLOC_CHECK(path_name, done);
		path_name[base - smb_fname->base_name] = '\0';
		base++;
	}

	/* original filename with path */
	DEBUG(10, ("recycle: fname = %s\n", smb_fname_str_dbg(smb_fname)));
	/* original path */
	DEBUG(10, ("recycle: fpath = %s\n", path_name));
	/* filename without path */
	DEBUG(10, ("recycle: base = %s\n", base));

	if (matchparam(recycle_exclude(handle), base)) {
		DEBUG(3, ("recycle: file %s is excluded \n", base));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	if (matchdirparam(recycle_exclude_dir(handle), path_name)) {
		DEBUG(3, ("recycle: directory %s is excluded \n", path_name));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	if (recycle_keep_dir_tree(handle) == True) {
		if (asprintf(&temp_name, "%s/%s", repository, path_name) == -1) {
			ALLOC_CHECK(temp_name, done);
		}
	} else {
		temp_name = SMB_STRDUP(repository);
	}
	ALLOC_CHECK(temp_name, done);

	exist = recycle_directory_exist(handle, temp_name);
	if (exist) {
		DEBUG(10, ("recycle: Directory already exists\n"));
	} else {
		DEBUG(10, ("recycle: Creating directory %s\n", temp_name));
		if (recycle_create_dir(handle, temp_name) == False) {
			DEBUG(3, ("recycle: Could not create directory, "
				  "purging %s...\n",
				  smb_fname_str_dbg(smb_fname)));
			rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
			goto done;
		}
	}

	if (asprintf(&final_name, "%s/%s", temp_name, base) == -1) {
		ALLOC_CHECK(final_name, done);
	}

	/* Create smb_fname with final base name and orig stream name. */
	status = create_synthetic_smb_fname(talloc_tos(), final_name,
					    smb_fname->stream_name, NULL,
					    &smb_fname_final);
	if (!NT_STATUS_IS_OK(status)) {
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	/* new filename with path */
	DEBUG(10, ("recycle: recycled file name: %s\n",
		   smb_fname_str_dbg(smb_fname_final)));

	/* check if we should delete file from recycle bin */
	if (recycle_file_exist(handle, smb_fname_final)) {
		if (recycle_versions(handle) == False || matchparam(recycle_noversions(handle), base) == True) {
			DEBUG(3, ("recycle: Removing old file %s from recycle "
				  "bin\n", smb_fname_str_dbg(smb_fname_final)));
			if (SMB_VFS_NEXT_UNLINK(handle,
						smb_fname_final) != 0) {
				DEBUG(1, ("recycle: Error deleting old file: %s\n", strerror(errno)));
			}
		}
	}

	/* rename file we move to recycle bin */
	i = 1;
	while (recycle_file_exist(handle, smb_fname_final)) {
		SAFE_FREE(final_name);
		if (asprintf(&final_name, "%s/Copy #%d of %s", temp_name, i++, base) == -1) {
			ALLOC_CHECK(final_name, done);
		}
		TALLOC_FREE(smb_fname_final->base_name);
		smb_fname_final->base_name = talloc_strdup(smb_fname_final,
							   final_name);
		if (smb_fname_final->base_name == NULL) {
			rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
			goto done;
		}
	}

	DEBUG(10, ("recycle: Moving %s to %s\n", smb_fname_str_dbg(smb_fname),
		smb_fname_str_dbg(smb_fname_final)));
	rc = SMB_VFS_NEXT_RENAME(handle, smb_fname, smb_fname_final);
	if (rc != 0) {
		DEBUG(3, ("recycle: Move error %d (%s), purging file %s "
			  "(%s)\n", errno, strerror(errno),
			  smb_fname_str_dbg(smb_fname),
			  smb_fname_str_dbg(smb_fname_final)));
		rc = SMB_VFS_NEXT_UNLINK(handle, smb_fname);
		goto done;
	}

	/* touch access date of moved file */
	if (recycle_touch(handle) == True || recycle_touch_mtime(handle))
		recycle_do_touch(handle, smb_fname_final,
				 recycle_touch_mtime(handle));

done:
	SAFE_FREE(path_name);
	SAFE_FREE(temp_name);
	SAFE_FREE(final_name);
	TALLOC_FREE(smb_fname_final);
	TALLOC_FREE(repository);
	return rc;
}

static struct vfs_fn_pointers vfs_recycle_fns = {
	.connect_fn = recycle_connect,
	.disconnect = recycle_disconnect,
	.unlink = recycle_unlink
};

NTSTATUS vfs_recycle_init(void);
NTSTATUS vfs_recycle_init(void)
{
	NTSTATUS ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "recycle",
					&vfs_recycle_fns);

	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_recycle_debug_level = debug_add_class("recycle");
	if (vfs_recycle_debug_level == -1) {
		vfs_recycle_debug_level = DBGC_VFS;
		DEBUG(0, ("vfs_recycle: Couldn't register custom debugging class!\n"));
	} else {
		DEBUG(10, ("vfs_recycle: Debug class number of 'recycle': %d\n", vfs_recycle_debug_level));
	}

	return ret;
}
