/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - 

   Copyright (C) Andrew Tridgell 2004

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
#include "vfs_posix.h"

/****************************************************************************
 Change a unix mode to a dos mode.
****************************************************************************/
static uint32_t dos_mode_from_stat(struct pvfs_state *pvfs, struct stat *st)
{
	int result = 0;

	if ((st->st_mode & S_IWUSR) == 0)
		result |= FILE_ATTRIBUTE_READONLY;
	
	if ((pvfs->flags & PVFS_FLAG_MAP_ARCHIVE) && ((st->st_mode & S_IXUSR) != 0))
		result |= FILE_ATTRIBUTE_ARCHIVE;
	
	if ((pvfs->flags & PVFS_FLAG_MAP_SYSTEM) && ((st->st_mode & S_IXGRP) != 0))
		result |= FILE_ATTRIBUTE_SYSTEM;
	
	if ((pvfs->flags & PVFS_FLAG_MAP_HIDDEN) && ((st->st_mode & S_IXOTH) != 0))
		result |= FILE_ATTRIBUTE_HIDDEN;
  
	if (S_ISDIR(st->st_mode))
		result = FILE_ATTRIBUTE_DIRECTORY | (result & FILE_ATTRIBUTE_READONLY);

	return result;
}



/*
  fill in the dos file attributes for a file
*/
NTSTATUS pvfs_fill_dos_info(struct pvfs_state *pvfs, struct pvfs_filename *name,
			    uint_t flags, int fd)
{
	NTSTATUS status;
	DATA_BLOB lkey;
	NTTIME write_time;

	/* make directories appear as size 0 with 1 link */
	if (S_ISDIR(name->st.st_mode)) {
		name->st.st_size = 0;
		name->st.st_nlink = 1;
	} else if (name->stream_id == 0) {
		name->stream_name = NULL;
	}

	/* for now just use the simple samba mapping */
	unix_to_nt_time(&name->dos.create_time, name->st.st_ctime);
	unix_to_nt_time(&name->dos.access_time, name->st.st_atime);
	unix_to_nt_time(&name->dos.write_time,  name->st.st_mtime);
	unix_to_nt_time(&name->dos.change_time, name->st.st_ctime);
#ifdef HAVE_STAT_TV_NSEC
	name->dos.create_time += name->st.st_ctim.tv_nsec / 100;
	name->dos.access_time += name->st.st_atim.tv_nsec / 100;
	name->dos.write_time  += name->st.st_mtim.tv_nsec / 100;
	name->dos.change_time += name->st.st_ctim.tv_nsec / 100;
#endif
	name->dos.attrib = dos_mode_from_stat(pvfs, &name->st);
	name->dos.alloc_size = pvfs_round_alloc_size(pvfs, name->st.st_size);
	name->dos.nlink = name->st.st_nlink;
	name->dos.ea_size = 4;
	if (pvfs->ntvfs->ctx->protocol == PROTOCOL_SMB2) {
		/* SMB2 represents a null EA with zero bytes */
		name->dos.ea_size = 0;
	}
	
	name->dos.file_id = (((uint64_t)name->st.st_dev)<<32) | name->st.st_ino;
	name->dos.flags = 0;

	status = pvfs_dosattrib_load(pvfs, name, fd);
	NT_STATUS_NOT_OK_RETURN(status);

	if (flags & PVFS_RESOLVE_NO_OPENDB) {
		return NT_STATUS_OK;
	}

	status = pvfs_locking_key(name, name, &lkey);
	NT_STATUS_NOT_OK_RETURN(status);

	status = odb_get_file_infos(pvfs->odb_context, &lkey,
				    NULL, &write_time);
	data_blob_free(&lkey);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("WARNING: odb_get_file_infos: %s\n", nt_errstr(status)));
		return status;
	}

	if (!null_time(write_time)) {
		name->dos.write_time = write_time;
	}

	return NT_STATUS_OK;
}


/*
  return a set of unix file permissions for a new file or directory
*/
mode_t pvfs_fileperms(struct pvfs_state *pvfs, uint32_t attrib)
{
	mode_t mode = (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE) &&
	    (attrib & FILE_ATTRIBUTE_READONLY)) {
		mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	}

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		if ((attrib & FILE_ATTRIBUTE_ARCHIVE) &&
		    (pvfs->flags & PVFS_FLAG_MAP_ARCHIVE)) {
			mode |= S_IXUSR;
		}
		if ((attrib & FILE_ATTRIBUTE_SYSTEM) &&
		    (pvfs->flags & PVFS_FLAG_MAP_SYSTEM)) {
			mode |= S_IXGRP;
		}
		if ((attrib & FILE_ATTRIBUTE_HIDDEN) &&
		    (pvfs->flags & PVFS_FLAG_MAP_HIDDEN)) {
			mode |= S_IXOTH;
		}
	}

	if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
		mode |= (S_IFDIR | S_IWUSR);
		mode |= (S_IXUSR | S_IXGRP | S_IXOTH);                 
		mode &= pvfs->options.dir_mask;
		mode |= pvfs->options.force_dir_mode;
	} else {
		mode &= pvfs->options.create_mask;
		mode |= pvfs->options.force_create_mode;
	}

	return mode;
}


