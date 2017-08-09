/*
 * Catia VFS module
 *
 * Implement a fixed mapping of forbidden NT characters in filenames that are
 * used a lot by the CAD package Catia.
 *
 * Yes, this a BAD BAD UGLY INCOMPLETE hack, but it helps quite some people
 * out there. Catia V4 on AIX uses characters like "<*$ a *lot*, all forbidden
 * under Windows...
 *
 * Copyright (C) Volker Lendecke, 2005
 * Copyright (C) Aravind Srinivasan, 2009
 * Copyright (C) Guenter Kukkukk, 2013
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

static int vfs_catia_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_catia_debug_level

#define GLOBAL_SNUM     0xFFFFFFF
#define MAP_SIZE        0xFF
#define MAP_NUM         0x101 /* max unicode charval / MAP_SIZE */
#define T_OFFSET(_v_)   ((_v_ % MAP_SIZE))
#define T_START(_v_)    (((_v_ / MAP_SIZE) * MAP_SIZE))
#define T_PICK(_v_)     ((_v_ / MAP_SIZE))

struct char_mappings {
	smb_ucs2_t entry[MAP_SIZE][2];
};

struct share_mapping_entry {
	int snum;
	struct share_mapping_entry *next;
	struct char_mappings **mappings;
};

struct share_mapping_entry *srt_head = NULL;

static bool build_table(struct char_mappings **cmaps, int value)
{
	int i;
	int start = T_START(value);

	(*cmaps) = talloc_zero(NULL, struct char_mappings);

	if (!*cmaps)
		return False;

	for (i = 0; i < MAP_SIZE;i++) {
		(*cmaps)->entry[i][vfs_translate_to_unix] = start + i;
		(*cmaps)->entry[i][vfs_translate_to_windows] = start + i;
	}

	return True;
}

static void set_tables(struct char_mappings **cmaps,
		       long unix_map,
		       long windows_map)
{
	int i;

	/* set unix -> windows */
	i = T_OFFSET(unix_map);
	cmaps[T_PICK(unix_map)]->entry[i][vfs_translate_to_windows] = windows_map;

	/* set windows -> unix */
	i = T_OFFSET(windows_map);
	cmaps[T_PICK(windows_map)]->entry[i][vfs_translate_to_unix] = unix_map;
}

static bool build_ranges(struct char_mappings **cmaps,
			 long unix_map,
			 long windows_map)
{

	if (!cmaps[T_PICK(unix_map)]) {
		if (!build_table(&cmaps[T_PICK(unix_map)], unix_map))
			return False;
	}

	if (!cmaps[T_PICK(windows_map)]) {
		if (!build_table(&cmaps[T_PICK(windows_map)], windows_map))
			return False;
	}

	set_tables(cmaps, unix_map, windows_map);

	return True;
}

static struct share_mapping_entry *get_srt(connection_struct *conn,
					   struct share_mapping_entry **global)
{
	struct share_mapping_entry *share;

	for (share = srt_head; share != NULL; share = share->next) {
		if (share->snum == GLOBAL_SNUM)
			(*global) = share;

		if (share->snum == SNUM(conn))
			return share;
	}

	return share;
}

static struct share_mapping_entry *add_srt(int snum, const char **mappings)
{

	char *tmp;
	fstring mapping;
	int i;
	long unix_map, windows_map;
	struct share_mapping_entry *ret = NULL;

	ret = (struct share_mapping_entry *)
		TALLOC_ZERO(NULL, sizeof(struct share_mapping_entry) +
		(mappings ? (MAP_NUM * sizeof(struct char_mappings *)) : 0));

	if (!ret)
		return ret;

	ret->snum = snum;

	if (mappings) {
		ret->mappings = (struct char_mappings**) ((unsigned char*) ret +
		    sizeof(struct share_mapping_entry));
		memset(ret->mappings, 0,
		    MAP_NUM * sizeof(struct char_mappings *));
	} else {
		ret->mappings = NULL;
		return ret;
	}

	/*
	 * catia mappings are of the form :
	 * UNIX char (in 0xnn hex) : WINDOWS char (in 0xnn hex)
	 *
	 * multiple mappings are comma seperated in smb.conf
	 */
	for (i=0;mappings[i];i++) {
		fstrcpy(mapping, mappings[i]);
		unix_map = strtol(mapping, &tmp, 16);
		if (unix_map == 0 && errno == EINVAL) {
			DEBUG(0, ("INVALID CATIA MAPPINGS - %s\n", mapping));
			continue;
		}
		windows_map = strtol(++tmp, NULL, 16);
		if (windows_map == 0 && errno == EINVAL) {
			DEBUG(0, ("INVALID CATIA MAPPINGS - %s\n", mapping));
			continue;
		}

		if (!build_ranges(ret->mappings, unix_map, windows_map)) {
			DEBUG(0, ("TABLE ERROR - CATIA MAPPINGS - %s\n", mapping));
			continue;
		}
	}

	ret->next = srt_head;
	srt_head = ret;

	return ret;
}

static bool init_mappings(connection_struct *conn,
			  struct share_mapping_entry **selected_out)
{
	const char **mappings = NULL;
	struct share_mapping_entry *share_level = NULL;
	struct share_mapping_entry *global = NULL;

	/* check srt cache */
	share_level = get_srt(conn, &global);
	if (share_level) {
		*selected_out = share_level;
		return (share_level->mappings != NULL);
	}

	/* see if we have a global setting */
	if (!global) {
		/* global setting */
		mappings = lp_parm_string_list(-1, "catia", "mappings", NULL);
		global = add_srt(GLOBAL_SNUM, mappings);
	}

	/* no global setting - what about share level ? */
	mappings = lp_parm_string_list(SNUM(conn), "catia", "mappings", NULL);
	share_level = add_srt(SNUM(conn), mappings);

	if (share_level->mappings) {
		(*selected_out) = share_level;
		return True;
	} else if (global->mappings) {
		share_level->mappings = global->mappings;
		(*selected_out) = share_level;
		return True;
	}

	return False;
}

static NTSTATUS catia_string_replace_allocate(connection_struct *conn,
					      const char *name_in,
					      char **mapped_name,
					      int direction)
{
	static smb_ucs2_t *tmpbuf = NULL;
	smb_ucs2_t *ptr;
	struct share_mapping_entry *selected;
	struct char_mappings *map = NULL;
	size_t converted_size;
	TALLOC_CTX *ctx = talloc_tos();

	if (!init_mappings(conn, &selected)) {
		/* No mappings found. Just use the old name */
		*mapped_name = talloc_strdup(NULL, name_in);
		if (!*mapped_name) {
			errno = ENOMEM;
			return NT_STATUS_NO_MEMORY;
		}
		return NT_STATUS_OK;
	}

	if ((push_ucs2_talloc(ctx, &tmpbuf, name_in,
			      &converted_size)) == false) {
		return map_nt_error_from_unix(errno);
	}
	ptr = tmpbuf;
	for(;*ptr;ptr++) {
		if (*ptr == 0)
			break;
		map = selected->mappings[T_PICK((*ptr))];

		/* nothing to do */
		if (!map)
			continue;

		*ptr = map->entry[T_OFFSET((*ptr))][direction];
	}

	if ((pull_ucs2_talloc(ctx, mapped_name, tmpbuf,
			      &converted_size)) == false) {
		TALLOC_FREE(tmpbuf);
		return map_nt_error_from_unix(errno);
	}
	TALLOC_FREE(tmpbuf);
	return NT_STATUS_OK;
}

static SMB_STRUCT_DIR *catia_opendir(vfs_handle_struct *handle,
				     const char *fname,
				     const char *mask,
				     uint32 attr)
{
	char *name_mapped = NULL;
	NTSTATUS status;
	SMB_STRUCT_DIR *ret;

	status = catia_string_replace_allocate(handle->conn, fname,
					&name_mapped, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return NULL;
	}

	ret = SMB_VFS_NEXT_OPENDIR(handle, name_mapped, mask, attr);
	TALLOC_FREE(name_mapped);

	return ret;
}

/*
 * TRANSLATE_NAME call which converts the given name to
 * "WINDOWS displayable" name
 */
static NTSTATUS catia_translate_name(struct vfs_handle_struct *handle,
				     const char *orig_name,
				     enum vfs_translate_direction direction,
				     TALLOC_CTX *mem_ctx,
				     char **pmapped_name)
{
	char *name = NULL;
	char *mapped_name;
	NTSTATUS status, ret;

	/*
	 * Copy the supplied name and free the memory for mapped_name,
	 * already allocated by the caller.
	 * We will be allocating new memory for mapped_name in
	 * catia_string_replace_allocate
	 */
	name = talloc_strdup(talloc_tos(), orig_name);
	if (!name) {
		errno = ENOMEM;
		return NT_STATUS_NO_MEMORY;
	}
	status = catia_string_replace_allocate(handle->conn, name,
			&mapped_name, direction);

	TALLOC_FREE(name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ret = SMB_VFS_NEXT_TRANSLATE_NAME(handle, mapped_name, direction,
					  mem_ctx, pmapped_name);

	if (NT_STATUS_EQUAL(ret, NT_STATUS_NONE_MAPPED)) {
		*pmapped_name = talloc_move(mem_ctx, &mapped_name);
		/* we need to return the former translation result here */
		ret = status;
	} else {
		TALLOC_FREE(mapped_name);
	}

	return ret;
}

static int catia_open(vfs_handle_struct *handle,
		      struct smb_filename *smb_fname,
		      files_struct *fsp,
		      int flags,
		      mode_t mode)
{
	char *name_mapped = NULL;
	char *tmp_base_name;
	int ret;
	NTSTATUS status;

	tmp_base_name = smb_fname->base_name;
	status = catia_string_replace_allocate(handle->conn,
					smb_fname->base_name,
					&name_mapped, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	smb_fname->base_name = name_mapped;
	ret = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	smb_fname->base_name = tmp_base_name;
	TALLOC_FREE(name_mapped);

	return ret;
}

static int catia_rename(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname_src,
			const struct smb_filename *smb_fname_dst)
{
	TALLOC_CTX *ctx = talloc_tos();
	struct smb_filename *smb_fname_src_tmp = NULL;
	struct smb_filename *smb_fname_dst_tmp = NULL;
	char *src_name_mapped = NULL;
	char *dst_name_mapped = NULL;
	NTSTATUS status;
	int ret = -1;

	status = catia_string_replace_allocate(handle->conn,
				smb_fname_src->base_name,
				&src_name_mapped, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	status = catia_string_replace_allocate(handle->conn,
				smb_fname_dst->base_name,
				&dst_name_mapped, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/* Setup temporary smb_filename structs. */
	status = copy_smb_filename(ctx, smb_fname_src, &smb_fname_src_tmp);

	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	status = copy_smb_filename(ctx, smb_fname_dst, &smb_fname_dst_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	smb_fname_src_tmp->base_name = src_name_mapped;
	smb_fname_dst_tmp->base_name = dst_name_mapped;	
	DEBUG(10, ("converted old name: %s\n",
				smb_fname_str_dbg(smb_fname_src_tmp)));
	DEBUG(10, ("converted new name: %s\n",
				smb_fname_str_dbg(smb_fname_dst_tmp)));

	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src_tmp,
			smb_fname_dst_tmp);
out:
	TALLOC_FREE(src_name_mapped);
	TALLOC_FREE(dst_name_mapped);
	TALLOC_FREE(smb_fname_src_tmp);
	TALLOC_FREE(smb_fname_dst_tmp);
	return ret;
}

static int catia_stat(vfs_handle_struct *handle,
		      struct smb_filename *smb_fname)
{
	char *name = NULL;
	char *tmp_base_name;
	int ret;
	NTSTATUS status;

	status = catia_string_replace_allocate(handle->conn,
				smb_fname->base_name,
				&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	tmp_base_name = smb_fname->base_name;
	smb_fname->base_name = name;

	ret = SMB_VFS_NEXT_STAT(handle, smb_fname);
	smb_fname->base_name = tmp_base_name;

	TALLOC_FREE(name);
	return ret;
}

static int catia_lstat(vfs_handle_struct *handle,
		       struct smb_filename *smb_fname)
{
	char *name = NULL;
	char *tmp_base_name;
	int ret;
	NTSTATUS status;

	status = catia_string_replace_allocate(handle->conn,
				smb_fname->base_name,
				&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	tmp_base_name = smb_fname->base_name;
	smb_fname->base_name = name;

	ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname);
	smb_fname->base_name = tmp_base_name;
	TALLOC_FREE(name);

	return ret;
}

static int catia_unlink(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn,
					smb_fname->base_name,
					&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	/* Setup temporary smb_filename structs. */
	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	smb_fname_tmp->base_name = name;
	ret = SMB_VFS_NEXT_UNLINK(handle, smb_fname_tmp);
	TALLOC_FREE(smb_fname_tmp);
	TALLOC_FREE(name);

	return ret;
}

static int catia_chown(vfs_handle_struct *handle,
		       const char *path,
		       uid_t uid,
		       gid_t gid)
{
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
					&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_CHOWN(handle, name, uid, gid);
	TALLOC_FREE(name);

	return ret;
}

static int catia_lchown(vfs_handle_struct *handle,
			const char *path,
			uid_t uid,
			gid_t gid)
{
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
					&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_LCHOWN(handle, name, uid, gid);
	TALLOC_FREE(name);

	return ret;
}

static int catia_rmdir(vfs_handle_struct *handle,
		       const char *path)
{
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
					&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_RMDIR(handle, name);
	TALLOC_FREE(name);

	return ret;
}

static int catia_mkdir(vfs_handle_struct *handle,
		       const char *path,
		       mode_t mode)
{
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
					 &name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_MKDIR(handle, name, mode);
	TALLOC_FREE(name);

	return ret;
}

static int catia_chdir(vfs_handle_struct *handle,
		       const char *path)
{
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
					&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_CHDIR(handle, name);
	TALLOC_FREE(name);

	return ret;
}

static int catia_ntimes(vfs_handle_struct *handle,
			const struct smb_filename *smb_fname,
			struct smb_file_time *ft)
{
	struct smb_filename *smb_fname_tmp = NULL;
	char *name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn,
				smb_fname->base_name,
				&name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	status = copy_smb_filename(talloc_tos(), smb_fname, &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	smb_fname_tmp->base_name = name;
	ret = SMB_VFS_NEXT_NTIMES(handle, smb_fname_tmp, ft);
	TALLOC_FREE(name);
	TALLOC_FREE(smb_fname_tmp);

	return ret;
}

static char *
catia_realpath(vfs_handle_struct *handle, const char *path)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	char *ret = NULL;

	status = catia_string_replace_allocate(handle->conn, path,
				        &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return NULL;
	}

	ret = SMB_VFS_NEXT_REALPATH(handle, mapped_name);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int catia_chflags(struct vfs_handle_struct *handle,
			 const char *path, unsigned int flags)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn, path,
				        &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_CHFLAGS(handle, mapped_name, flags);
	TALLOC_FREE(mapped_name);

	return ret;
}

static NTSTATUS
catia_streaminfo(struct vfs_handle_struct *handle,
		 struct files_struct *fsp,
		 const char *path,
		 TALLOC_CTX *mem_ctx,
		 unsigned int *num_streams,
		 struct stream_struct **streams)
{
	char *mapped_name = NULL;
	NTSTATUS status;

	status = catia_string_replace_allocate(handle->conn, path,
				        &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return status;
	}

	status = SMB_VFS_NEXT_STREAMINFO(handle, fsp, mapped_name,
					 mem_ctx, num_streams,streams);
	TALLOC_FREE(mapped_name);

	return status;
}

static NTSTATUS
catia_get_nt_acl(struct vfs_handle_struct *handle,
		 const char *path,
		 uint32 security_info,
		 struct security_descriptor **ppdesc)
{
	char *mapped_name = NULL;
	NTSTATUS status;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return status;
	}
	status = SMB_VFS_NEXT_GET_NT_ACL(handle, mapped_name,
					 security_info, ppdesc);
	TALLOC_FREE(mapped_name);

	return status;
}

static int
catia_chmod_acl(vfs_handle_struct *handle,
		const char *path,
		mode_t mode)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_CHMOD_ACL(handle, mapped_name, mode);
	TALLOC_FREE(mapped_name);
	return ret;
}

static SMB_ACL_T
catia_sys_acl_get_file(vfs_handle_struct *handle,
		       const char *path,
		       SMB_ACL_TYPE_T type)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	SMB_ACL_T ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return NULL;
	}

	ret = SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, mapped_name, type);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_sys_acl_set_file(vfs_handle_struct *handle,
		       const char *path,
		       SMB_ACL_TYPE_T type,
		       SMB_ACL_T theacl)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, mapped_name, type, theacl);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_sys_acl_delete_def_file(vfs_handle_struct *handle,
			      const char *path)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	int ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	ret = SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, mapped_name);
	TALLOC_FREE(mapped_name);

	return ret;
}

static ssize_t
catia_getxattr(vfs_handle_struct *handle, const char *path,
	       const char *name, void *value, size_t size)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_GETXATTR(handle, path, mapped_name, value, size);
	TALLOC_FREE(mapped_name);

	return ret;
}

static ssize_t
catia_lgetxattr(vfs_handle_struct *handle, const char *path,
		const char *name, void *value, size_t size)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_LGETXATTR(handle, path, mapped_name, value, size);
	TALLOC_FREE(mapped_name);

	return ret;
}

static ssize_t
catia_listxattr(vfs_handle_struct *handle, const char *path,
		char *list, size_t size)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_LISTXATTR(handle, mapped_name, list, size);
	TALLOC_FREE(mapped_name);

	return ret;
}

static ssize_t
catia_llistxattr(vfs_handle_struct *handle, const char *path,
		 char *list, size_t size)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				path, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_LLISTXATTR(handle, mapped_name, list, size);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_removexattr(vfs_handle_struct *handle, const char *path,
		  const char *name)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_REMOVEXATTR(handle, path, mapped_name);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_lremovexattr(vfs_handle_struct *handle, const char *path,
		   const char *name)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_LREMOVEXATTR(handle, path, mapped_name);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_setxattr(vfs_handle_struct *handle, const char *path,
	       const char *name, const void *value, size_t size,
	       int flags)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_SETXATTR(handle, path, mapped_name, value, size, flags);
	TALLOC_FREE(mapped_name);

	return ret;
}

static int
catia_lsetxattr(vfs_handle_struct *handle, const char *path,
		const char *name, const void *value, size_t size,
		int flags)
{
	char *mapped_name = NULL;
	NTSTATUS status;
	ssize_t ret;

	status = catia_string_replace_allocate(handle->conn,
				name, &mapped_name, vfs_translate_to_unix);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}


	ret = SMB_VFS_NEXT_LSETXATTR(handle, path, mapped_name, value, size, flags);
	TALLOC_FREE(mapped_name);

	return ret;
}

static struct vfs_fn_pointers vfs_catia_fns = {
        .mkdir = catia_mkdir,
        .rmdir = catia_rmdir,
        .opendir = catia_opendir,
        .open_fn = catia_open,
        .rename = catia_rename,
        .stat = catia_stat,
        .lstat = catia_lstat,
        .unlink = catia_unlink,
        .chown = catia_chown,
        .lchown = catia_lchown,
        .chdir = catia_chdir,
	.ntimes = catia_ntimes,
	.realpath = catia_realpath,
	.chflags = catia_chflags,
	.streaminfo = catia_streaminfo,
	.translate_name = catia_translate_name,
	.get_nt_acl = catia_get_nt_acl,
	.chmod_acl = catia_chmod_acl,
	.sys_acl_get_file = catia_sys_acl_get_file,
	.sys_acl_set_file = catia_sys_acl_set_file,
	.sys_acl_delete_def_file = catia_sys_acl_delete_def_file,
	.getxattr = catia_getxattr,
	.lgetxattr = catia_lgetxattr,
	.listxattr = catia_listxattr,
	.llistxattr = catia_llistxattr,
	.removexattr = catia_removexattr,
	.lremovexattr = catia_lremovexattr,
	.setxattr = catia_setxattr,
	.lsetxattr = catia_lsetxattr,
};

NTSTATUS vfs_catia_init(void)
{
	NTSTATUS ret;

        ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "catia",
				&vfs_catia_fns);
	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_catia_debug_level = debug_add_class("catia");
	if (vfs_catia_debug_level == -1) {
		vfs_catia_debug_level = DBGC_VFS;
		DEBUG(0, ("vfs_catia: Couldn't register custom debugging "
			  "class!\n"));
	} else {
		DEBUG(10, ("vfs_catia: Debug class number of "
			   "'catia': %d\n", vfs_catia_debug_level));
	}

	return ret;

}
