/* 
   Unix SMB/CIFS implementation.

   simple NTVFS filesystem backend

   Copyright (C) Andrew Tridgell 2003

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
/*
  this implements a very simple NTVFS filesystem backend. 
  
  this backend largely ignores the POSIX -> CIFS mappings, just doing absolutely
  minimal work to give a working backend.
*/

#include "includes.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "svfs.h"
#include "system/time.h"
#include "../lib/util/dlinklist.h"
#include "ntvfs/ntvfs.h"
#include "ntvfs/simple/proto.h"

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#define CHECK_READ_ONLY(req) do { if (share_bool_option(ntvfs->ctx->config, SHARE_READONLY, true)) return NT_STATUS_ACCESS_DENIED; } while (0)

/*
  connect to a share - used when a tree_connect operation comes
  in. For a disk based backend we needs to ensure that the base
  directory exists (tho it doesn't need to be accessible by the user,
  that comes later)
*/
static NTSTATUS svfs_connect(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_tcon* tcon)
{
	struct stat st;
	struct svfs_private *p;
	struct share_config *scfg = ntvfs->ctx->config;
	const char *sharename;

	switch (tcon->generic.level) {
	case RAW_TCON_TCON:
		sharename = tcon->tcon.in.service;
		break;
	case RAW_TCON_TCONX:
		sharename = tcon->tconx.in.path;
		break;
	case RAW_TCON_SMB2:
		sharename = tcon->smb2.in.path;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	if (strncmp(sharename, "\\\\", 2) == 0) {
		char *p2 = strchr(sharename+2, '\\');
		if (p2) {
			sharename = p2 + 1;
		}
	}

	p = talloc(ntvfs, struct svfs_private);
	NT_STATUS_HAVE_NO_MEMORY(p);
	p->ntvfs = ntvfs;
	p->next_search_handle = 0;
	p->connectpath = talloc_strdup(p, share_string_option(scfg, SHARE_PATH, ""));
	p->open_files = NULL;
	p->search = NULL;

	/* the directory must exist */
	if (stat(p->connectpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
		DEBUG(0,("'%s' is not a directory, when connecting to [%s]\n", 
			 p->connectpath, sharename));
		return NT_STATUS_BAD_NETWORK_NAME;
	}

	ntvfs->ctx->fs_type = talloc_strdup(ntvfs->ctx, "NTFS");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->fs_type);
	ntvfs->ctx->dev_type = talloc_strdup(ntvfs->ctx, "A:");
	NT_STATUS_HAVE_NO_MEMORY(ntvfs->ctx->dev_type);

	if (tcon->generic.level == RAW_TCON_TCONX) {
		tcon->tconx.out.fs_type = ntvfs->ctx->fs_type;
		tcon->tconx.out.dev_type = ntvfs->ctx->dev_type;
	}

	ntvfs->private_data = p;

	return NT_STATUS_OK;
}

/*
  disconnect from a share
*/
static NTSTATUS svfs_disconnect(struct ntvfs_module_context *ntvfs)
{
	return NT_STATUS_OK;
}

/*
  find open file handle given fd
*/
static struct svfs_file *find_fd(struct svfs_private *sp, struct ntvfs_handle *handle)
{
	struct svfs_file *f;
	void *p;

	p = ntvfs_handle_get_backend_data(handle, sp->ntvfs);
	if (!p) return NULL;

	f = talloc_get_type(p, struct svfs_file);
	if (!f) return NULL;

	return f;
}

/*
  delete a file - the dirtype specifies the file types to include in the search. 
  The name can contain CIFS wildcards, but rarely does (except with OS/2 clients)
*/
static NTSTATUS svfs_unlink(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req,
			    union smb_unlink *unl)
{
	char *unix_path;

	CHECK_READ_ONLY(req);

	unix_path = svfs_unix_path(ntvfs, req, unl->unlink.in.pattern);

	/* ignoring wildcards ... */
	if (unlink(unix_path) == -1) {
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}


/*
  ioctl interface - we don't do any
*/
static NTSTATUS svfs_ioctl(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_ioctl *io)
{
	return NT_STATUS_INVALID_PARAMETER;
}

/*
  check if a directory exists
*/
static NTSTATUS svfs_chkpath(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req,
			     union smb_chkpath *cp)
{
	char *unix_path;
	struct stat st;

	unix_path = svfs_unix_path(ntvfs, req, cp->chkpath.in.path);

	if (stat(unix_path, &st) == -1) {
		return map_nt_error_from_unix(errno);
	}

	if (!S_ISDIR(st.st_mode)) {
		return NT_STATUS_NOT_A_DIRECTORY;
	}

	return NT_STATUS_OK;
}

/*
  build a file_id from a stat struct
*/
static uint64_t svfs_file_id(struct stat *st)
{
	uint64_t ret = st->st_ino;
	ret <<= 32;
	ret |= st->st_dev;
	return ret;
}

/*
  approximately map a struct stat to a generic fileinfo struct
*/
static NTSTATUS svfs_map_fileinfo(struct ntvfs_module_context *ntvfs,
				  struct ntvfs_request *req, union smb_fileinfo *info, 
				  struct stat *st, const char *unix_path)
{
	struct svfs_dir *dir = NULL;
	char *pattern = NULL;
	int i;
	const char *s, *short_name;

	s = strrchr(unix_path, '/');
	if (s) {
		short_name = s+1;
	} else {
		short_name = "";
	}

	asprintf(&pattern, "%s:*", unix_path);
	
	if (pattern) {
		dir = svfs_list_unix(req, req, pattern);
	}

	unix_to_nt_time(&info->generic.out.create_time, st->st_ctime);
	unix_to_nt_time(&info->generic.out.access_time, st->st_atime);
	unix_to_nt_time(&info->generic.out.write_time,  st->st_mtime);
	unix_to_nt_time(&info->generic.out.change_time, st->st_mtime);
	info->generic.out.alloc_size = st->st_size;
	info->generic.out.size = st->st_size;
	info->generic.out.attrib = svfs_unix_to_dos_attrib(st->st_mode);
	info->generic.out.alloc_size = st->st_blksize * st->st_blocks;
	info->generic.out.nlink = st->st_nlink;
	info->generic.out.directory = S_ISDIR(st->st_mode) ? 1 : 0;
	info->generic.out.file_id = svfs_file_id(st);
	/* REWRITE: TODO stuff in here */
	info->generic.out.delete_pending = 0;
	info->generic.out.ea_size = 0;
	info->generic.out.num_eas = 0;
	info->generic.out.fname.s = talloc_strdup(req, short_name);
	info->generic.out.alt_fname.s = talloc_strdup(req, short_name);
	info->generic.out.compressed_size = 0;
	info->generic.out.format = 0;
	info->generic.out.unit_shift = 0;
	info->generic.out.chunk_shift = 0;
	info->generic.out.cluster_shift = 0;
	
	info->generic.out.access_flags = 0;
	info->generic.out.position = 0;
	info->generic.out.mode = 0;
	info->generic.out.alignment_requirement = 0;
	info->generic.out.reparse_tag = 0;
	info->generic.out.num_streams = 0;
	/* setup a single data stream */
	info->generic.out.num_streams = 1 + (dir?dir->count:0);
	info->generic.out.streams = talloc_array(req, 
						   struct stream_struct,
						   info->generic.out.num_streams);
	if (!info->generic.out.streams) {
		return NT_STATUS_NO_MEMORY;
	}
	info->generic.out.streams[0].size = st->st_size;
	info->generic.out.streams[0].alloc_size = st->st_size;
	info->generic.out.streams[0].stream_name.s = talloc_strdup(req,"::$DATA");

	for (i=0;dir && i<dir->count;i++) {
		s = strchr(dir->files[i].name, ':');
		info->generic.out.streams[1+i].size = dir->files[i].st.st_size;
		info->generic.out.streams[1+i].alloc_size = dir->files[i].st.st_size;
		info->generic.out.streams[1+i].stream_name.s = s?s:dir->files[i].name;
	}

	return NT_STATUS_OK;
}

/*
  return info on a pathname
*/
static NTSTATUS svfs_qpathinfo(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_fileinfo *info)
{
	char *unix_path;
	struct stat st;

	DEBUG(19,("svfs_qpathinfo: file %s level 0x%x\n", info->generic.in.file.path, info->generic.level));
	if (info->generic.level != RAW_FILEINFO_GENERIC) {
		return ntvfs_map_qpathinfo(ntvfs, req, info);
	}
	
	unix_path = svfs_unix_path(ntvfs, req, info->generic.in.file.path);
	DEBUG(19,("svfs_qpathinfo: file %s\n", unix_path));
	if (stat(unix_path, &st) == -1) {
		DEBUG(19,("svfs_qpathinfo: file %s errno=%d\n", unix_path, errno));
		return map_nt_error_from_unix(errno);
	}
	DEBUG(19,("svfs_qpathinfo: file %s, stat done\n", unix_path));
	return svfs_map_fileinfo(ntvfs, req, info, &st, unix_path);
}

/*
  query info on a open file
*/
static NTSTATUS svfs_qfileinfo(struct ntvfs_module_context *ntvfs,
			       struct ntvfs_request *req, union smb_fileinfo *info)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;
	struct stat st;

	if (info->generic.level != RAW_FILEINFO_GENERIC) {
		return ntvfs_map_qfileinfo(ntvfs, req, info);
	}

	f = find_fd(p, info->generic.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}
	
	if (fstat(f->fd, &st) == -1) {
		return map_nt_error_from_unix(errno);
	}

	return svfs_map_fileinfo(ntvfs, req,info, &st, f->name);
}


/*
  open a file
*/
static NTSTATUS svfs_open(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_open *io)
{
	struct svfs_private *p = ntvfs->private_data;
	char *unix_path;
	struct stat st;
	int fd, flags;
	struct svfs_file *f;
	int create_flags, rdwr_flags;
	bool readonly;
	NTSTATUS status;
	struct ntvfs_handle *handle;
	
	if (io->generic.level != RAW_OPEN_GENERIC) {
		return ntvfs_map_open(ntvfs, req, io);
	}

	readonly = share_bool_option(ntvfs->ctx->config, SHARE_READONLY, SHARE_READONLY_DEFAULT);
	if (readonly) {
		create_flags = 0;
		rdwr_flags = O_RDONLY;
	} else {
		create_flags = O_CREAT;
		rdwr_flags = O_RDWR;
	}

	unix_path = svfs_unix_path(ntvfs, req, io->ntcreatex.in.fname);

	switch (io->generic.in.open_disposition) {
	case NTCREATEX_DISP_SUPERSEDE:
	case NTCREATEX_DISP_OVERWRITE_IF:
		flags = create_flags | O_TRUNC;
		break;
	case NTCREATEX_DISP_OPEN:
	case NTCREATEX_DISP_OVERWRITE:
		flags = 0;
		break;
	case NTCREATEX_DISP_CREATE:
		flags = create_flags | O_EXCL;
		break;
	case NTCREATEX_DISP_OPEN_IF:
		flags = create_flags;
		break;
	default:
		flags = 0;
		break;
	}
	
	flags |= rdwr_flags;

	if (io->generic.in.create_options & NTCREATEX_OPTIONS_DIRECTORY) {
		flags = O_RDONLY | O_DIRECTORY;
		if (readonly) {
			goto do_open;
		}
		switch (io->generic.in.open_disposition) {
		case NTCREATEX_DISP_CREATE:
			if (mkdir(unix_path, 0755) == -1) {
				DEBUG(9,("svfs_open: mkdir %s errno=%d\n", unix_path, errno));
				return map_nt_error_from_unix(errno);
			}
			break;
		case NTCREATEX_DISP_OPEN_IF:
			if (mkdir(unix_path, 0755) == -1 && errno != EEXIST) {
				DEBUG(9,("svfs_open: mkdir %s errno=%d\n", unix_path, errno));
				return map_nt_error_from_unix(errno);
			}
			break;
		}
	}

do_open:
	fd = open(unix_path, flags, 0644);
	if (fd == -1) {
		return map_nt_error_from_unix(errno);
	}

	if (fstat(fd, &st) == -1) {
		DEBUG(9,("svfs_open: fstat errno=%d\n", errno));
		close(fd);
		return map_nt_error_from_unix(errno);
	}

	status = ntvfs_handle_new(ntvfs, req, &handle);
	NT_STATUS_NOT_OK_RETURN(status);

	f = talloc(handle, struct svfs_file);
	NT_STATUS_HAVE_NO_MEMORY(f);
	f->fd = fd;
	f->name = talloc_strdup(f, unix_path);
	NT_STATUS_HAVE_NO_MEMORY(f->name);

	DLIST_ADD(p->open_files, f);

	status = ntvfs_handle_set_backend_data(handle, ntvfs, f);
	NT_STATUS_NOT_OK_RETURN(status);

	ZERO_STRUCT(io->generic.out);
	
	unix_to_nt_time(&io->generic.out.create_time, st.st_ctime);
	unix_to_nt_time(&io->generic.out.access_time, st.st_atime);
	unix_to_nt_time(&io->generic.out.write_time,  st.st_mtime);
	unix_to_nt_time(&io->generic.out.change_time, st.st_mtime);
	io->generic.out.file.ntvfs = handle;
	io->generic.out.alloc_size = st.st_size;
	io->generic.out.size = st.st_size;
	io->generic.out.attrib = svfs_unix_to_dos_attrib(st.st_mode);
	io->generic.out.is_directory = S_ISDIR(st.st_mode) ? 1 : 0;

	return NT_STATUS_OK;
}

/*
  create a directory
*/
static NTSTATUS svfs_mkdir(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_mkdir *md)
{
	char *unix_path;

	CHECK_READ_ONLY(req);

	if (md->generic.level != RAW_MKDIR_MKDIR) {
		return NT_STATUS_INVALID_LEVEL;
	}

	unix_path = svfs_unix_path(ntvfs, req, md->mkdir.in.path);

	if (mkdir(unix_path, 0777) == -1) {
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}

/*
  remove a directory
*/
static NTSTATUS svfs_rmdir(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, struct smb_rmdir *rd)
{
	char *unix_path;

	CHECK_READ_ONLY(req);

	unix_path = svfs_unix_path(ntvfs, req, rd->in.path);

	if (rmdir(unix_path) == -1) {
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}

/*
  rename a set of files
*/
static NTSTATUS svfs_rename(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_rename *ren)
{
	char *unix_path1, *unix_path2;

	CHECK_READ_ONLY(req);

	if (ren->generic.level != RAW_RENAME_RENAME) {
		return NT_STATUS_INVALID_LEVEL;
	}

	unix_path1 = svfs_unix_path(ntvfs, req, ren->rename.in.pattern1);
	unix_path2 = svfs_unix_path(ntvfs, req, ren->rename.in.pattern2);

	if (rename(unix_path1, unix_path2) == -1) {
		return map_nt_error_from_unix(errno);
	}
	
	return NT_STATUS_OK;
}

/*
  copy a set of files
*/
static NTSTATUS svfs_copy(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, struct smb_copy *cp)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  read from a file
*/
static NTSTATUS svfs_read(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_read *rd)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;
	ssize_t ret;

	if (rd->generic.level != RAW_READ_READX) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	f = find_fd(p, rd->readx.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	ret = pread(f->fd, 
		    rd->readx.out.data, 
		    rd->readx.in.maxcnt,
		    rd->readx.in.offset);
	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}

	rd->readx.out.nread = ret;
	rd->readx.out.remaining = 0; /* should fill this in? */
	rd->readx.out.compaction_mode = 0; 

	return NT_STATUS_OK;
}

/*
  write to a file
*/
static NTSTATUS svfs_write(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_write *wr)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;
	ssize_t ret;

	if (wr->generic.level != RAW_WRITE_WRITEX) {
		return ntvfs_map_write(ntvfs, req, wr);
	}

	CHECK_READ_ONLY(req);

	f = find_fd(p, wr->writex.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	ret = pwrite(f->fd, 
		     wr->writex.in.data, 
		     wr->writex.in.count,
		     wr->writex.in.offset);
	if (ret == -1) {
		return map_nt_error_from_unix(errno);
	}
		
	wr->writex.out.nwritten = ret;
	wr->writex.out.remaining = 0; /* should fill this in? */
	
	return NT_STATUS_OK;
}

/*
  seek in a file
*/
static NTSTATUS svfs_seek(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_seek *io)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  flush a file
*/
static NTSTATUS svfs_flush(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_flush *io)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;

	switch (io->generic.level) {
	case RAW_FLUSH_FLUSH:
	case RAW_FLUSH_SMB2:
		/* ignore the additional unknown option in SMB2 */
		f = find_fd(p, io->generic.in.file.ntvfs);
		if (!f) {
			return NT_STATUS_INVALID_HANDLE;
		}
		fsync(f->fd);
		return NT_STATUS_OK;

	case RAW_FLUSH_ALL:
		for (f=p->open_files;f;f=f->next) {
			fsync(f->fd);
		}
		return NT_STATUS_OK;
	}

	return NT_STATUS_INVALID_LEVEL;
}

/*
  close a file
*/
static NTSTATUS svfs_close(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req,
			   union smb_close *io)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;

	if (io->generic.level != RAW_CLOSE_CLOSE) {
		/* we need a mapping function */
		return NT_STATUS_INVALID_LEVEL;
	}

	f = find_fd(p, io->close.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (close(f->fd) == -1) {
		return map_nt_error_from_unix(errno);
	}

	DLIST_REMOVE(p->open_files, f);
	talloc_free(f->name);
	talloc_free(f);

	return NT_STATUS_OK;
}

/*
  exit - closing files
*/
static NTSTATUS svfs_exit(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  logoff - closing files
*/
static NTSTATUS svfs_logoff(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/*
  setup for an async call
*/
static NTSTATUS svfs_async_setup(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, 
				 void *private_data)
{
	return NT_STATUS_OK;
}

/*
  cancel an async call
*/
static NTSTATUS svfs_cancel(struct ntvfs_module_context *ntvfs, struct ntvfs_request *req)
{
	return NT_STATUS_UNSUCCESSFUL;
}

/*
  lock a byte range
*/
static NTSTATUS svfs_lock(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_lock *lck)
{
	DEBUG(0,("REWRITE: not doing byte range locking!\n"));
	return NT_STATUS_OK;
}

/*
  set info on a pathname
*/
static NTSTATUS svfs_setpathinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_setfileinfo *st)
{
	CHECK_READ_ONLY(req);

	return NT_STATUS_NOT_SUPPORTED;
}

/*
  set info on a open file
*/
static NTSTATUS svfs_setfileinfo(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, 
				 union smb_setfileinfo *info)
{
	struct svfs_private *p = ntvfs->private_data;
	struct svfs_file *f;
	struct utimbuf unix_times;

	CHECK_READ_ONLY(req);

	f = find_fd(p, info->generic.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}
	
	switch (info->generic.level) {
	case RAW_SFILEINFO_END_OF_FILE_INFO:
	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		if (ftruncate(f->fd, 
			      info->end_of_file_info.in.size) == -1) {
			return map_nt_error_from_unix(errno);
		}
		break;
	case RAW_SFILEINFO_SETATTRE:
		unix_times.actime = info->setattre.in.access_time;
		unix_times.modtime = info->setattre.in.write_time;

		if (unix_times.actime == 0 && unix_times.modtime == 0) {
			break;
		} 

		/* set modify time = to access time if modify time was 0 */
		if (unix_times.actime != 0 && unix_times.modtime == 0) {
			unix_times.modtime = unix_times.actime;
		}

		/* Set the date on this file */
		if (svfs_file_utime(f->fd, &unix_times) != 0) {
			return NT_STATUS_ACCESS_DENIED;
		}
  		break;
	default:
		DEBUG(2,("svfs_setfileinfo: level %d not implemented\n", 
			 info->generic.level));
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return NT_STATUS_OK;
}


/*
  return filesystem space info
*/
static NTSTATUS svfs_fsinfo(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_fsinfo *fs)
{
	struct svfs_private *p = ntvfs->private_data;
	struct stat st;

	if (fs->generic.level != RAW_QFS_GENERIC) {
		return ntvfs_map_fsinfo(ntvfs, req, fs);
	}

	if (sys_fsusage(p->connectpath,
			&fs->generic.out.blocks_free, 
			&fs->generic.out.blocks_total) == -1) {
		return map_nt_error_from_unix(errno);
	}

	fs->generic.out.block_size = 512;

	if (stat(p->connectpath, &st) != 0) {
		return NT_STATUS_DISK_CORRUPT_ERROR;
	}
	
	fs->generic.out.fs_id = st.st_ino;
	unix_to_nt_time(&fs->generic.out.create_time, st.st_ctime);
	fs->generic.out.serial_number = st.st_ino;
	fs->generic.out.fs_attr = 0;
	fs->generic.out.max_file_component_length = 255;
	fs->generic.out.device_type = 0;
	fs->generic.out.device_characteristics = 0;
	fs->generic.out.quota_soft = 0;
	fs->generic.out.quota_hard = 0;
	fs->generic.out.quota_flags = 0;
	fs->generic.out.volume_name = talloc_strdup(req, ntvfs->ctx->config->name);
	fs->generic.out.fs_type = ntvfs->ctx->fs_type;

	return NT_STATUS_OK;
}

#if 0
/*
  return filesystem attribute info
*/
static NTSTATUS svfs_fsattr(struct ntvfs_module_context *ntvfs,
			    struct ntvfs_request *req, union smb_fsattr *fs)
{
	struct stat st;
	struct svfs_private *p = ntvfs->private_data;

	if (fs->generic.level != RAW_FSATTR_GENERIC) {
		return ntvfs_map_fsattr(ntvfs, req, fs);
	}

	if (stat(p->connectpath, &st) == -1) {
		return map_nt_error_from_unix(errno);
	}

	unix_to_nt_time(&fs->generic.out.create_time, st.st_ctime);
	fs->generic.out.fs_attr = 
		FILE_CASE_PRESERVED_NAMES | 
		FILE_CASE_SENSITIVE_SEARCH | 
		FILE_PERSISTENT_ACLS;
	fs->generic.out.max_file_component_length = 255;
	fs->generic.out.serial_number = 1;
	fs->generic.out.fs_type = talloc_strdup(req, "NTFS");
	fs->generic.out.volume_name = talloc_strdup(req, 
						    lpcfg_servicename(req->tcon->service));

	return NT_STATUS_OK;
}
#endif

/*
  return print queue info
*/
static NTSTATUS svfs_lpq(struct ntvfs_module_context *ntvfs,
			 struct ntvfs_request *req, union smb_lpq *lpq)
{
	return NT_STATUS_NOT_SUPPORTED;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS svfs_search_first(struct ntvfs_module_context *ntvfs,
				  struct ntvfs_request *req, union smb_search_first *io, 
				  void *search_private, 
				  bool (*callback)(void *, const union smb_search_data *))
{
	struct svfs_dir *dir;
	int i;
	struct svfs_private *p = ntvfs->private_data;
	struct search_state *search;
	union smb_search_data file;
	unsigned int max_count;

	if (io->generic.level != RAW_SEARCH_TRANS2) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (io->generic.data_level != RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	search = talloc_zero(p, struct search_state);
	if (!search) {
		return NT_STATUS_NO_MEMORY;
	}

	max_count = io->t2ffirst.in.max_count;

	dir = svfs_list(ntvfs, req, io->t2ffirst.in.pattern);
	if (!dir) {
		return NT_STATUS_FOOBAR;
	}

	search->handle = p->next_search_handle;
	search->dir = dir;

	if (dir->count < max_count) {
		max_count = dir->count;
	}

	for (i=0; i < max_count;i++) {
		ZERO_STRUCT(file);
		unix_to_nt_time(&file.both_directory_info.create_time, dir->files[i].st.st_ctime);
		unix_to_nt_time(&file.both_directory_info.access_time, dir->files[i].st.st_atime);
		unix_to_nt_time(&file.both_directory_info.write_time,  dir->files[i].st.st_mtime);
		unix_to_nt_time(&file.both_directory_info.change_time, dir->files[i].st.st_mtime);
		file.both_directory_info.name.s = dir->files[i].name;
		file.both_directory_info.short_name.s = dir->files[i].name;
		file.both_directory_info.size = dir->files[i].st.st_size;
		file.both_directory_info.attrib = svfs_unix_to_dos_attrib(dir->files[i].st.st_mode);

		if (!callback(search_private, &file)) {
			break;
		}
	}

	search->current_index = i;

	io->t2ffirst.out.count = i;
	io->t2ffirst.out.handle = search->handle;
	io->t2ffirst.out.end_of_search = (i == dir->count) ? 1 : 0;

	/* work out if we are going to keep the search state */
	if ((io->t2ffirst.in.flags & FLAG_TRANS2_FIND_CLOSE) ||
	    ((io->t2ffirst.in.flags & FLAG_TRANS2_FIND_CLOSE_IF_END) && (i == dir->count))) {
		talloc_free(search);
	} else {
		p->next_search_handle++;
		DLIST_ADD(p->search, search);
	}

	return NT_STATUS_OK;
}

/* continue a search */
static NTSTATUS svfs_search_next(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req, union smb_search_next *io, 
				 void *search_private, 
				 bool (*callback)(void *, const union smb_search_data *))
{
	struct svfs_dir *dir;
	int i;
	struct svfs_private *p = ntvfs->private_data;
	struct search_state *search;
	union smb_search_data file;
	unsigned int max_count;

	if (io->generic.level != RAW_SEARCH_TRANS2) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	if (io->generic.data_level != RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	for (search=p->search; search; search = search->next) {
		if (search->handle == io->t2fnext.in.handle) break;
	}
	
	if (!search) {
		/* we didn't find the search handle */
		return NT_STATUS_FOOBAR;
	}

	dir = search->dir;

	/* the client might be asking for something other than just continuing
	   with the search */
	if (!(io->t2fnext.in.flags & FLAG_TRANS2_FIND_CONTINUE) &&
	    (io->t2fnext.in.flags & FLAG_TRANS2_FIND_REQUIRE_RESUME) &&
	    io->t2fnext.in.last_name && *io->t2fnext.in.last_name) {
		/* look backwards first */
		for (i=search->current_index; i > 0; i--) {
			if (strcmp(io->t2fnext.in.last_name, dir->files[i-1].name) == 0) {
				search->current_index = i;
				goto found;
			}
		}

		/* then look forwards */
		for (i=search->current_index+1; i <= dir->count; i++) {
			if (strcmp(io->t2fnext.in.last_name, dir->files[i-1].name) == 0) {
				search->current_index = i;
				goto found;
			}
		}
	}

found:	
	max_count = search->current_index + io->t2fnext.in.max_count;

	if (max_count > dir->count) {
		max_count = dir->count;
	}

	for (i = search->current_index; i < max_count;i++) {
		ZERO_STRUCT(file);
		unix_to_nt_time(&file.both_directory_info.create_time, dir->files[i].st.st_ctime);
		unix_to_nt_time(&file.both_directory_info.access_time, dir->files[i].st.st_atime);
		unix_to_nt_time(&file.both_directory_info.write_time,  dir->files[i].st.st_mtime);
		unix_to_nt_time(&file.both_directory_info.change_time, dir->files[i].st.st_mtime);
		file.both_directory_info.name.s = dir->files[i].name;
		file.both_directory_info.short_name.s = dir->files[i].name;
		file.both_directory_info.size = dir->files[i].st.st_size;
		file.both_directory_info.attrib = svfs_unix_to_dos_attrib(dir->files[i].st.st_mode);

		if (!callback(search_private, &file)) {
			break;
		}
	}

	io->t2fnext.out.count = i - search->current_index;
	io->t2fnext.out.end_of_search = (i == dir->count) ? 1 : 0;

	search->current_index = i;

	/* work out if we are going to keep the search state */
	if ((io->t2fnext.in.flags & FLAG_TRANS2_FIND_CLOSE) ||
	    ((io->t2fnext.in.flags & FLAG_TRANS2_FIND_CLOSE_IF_END) && (i == dir->count))) {
		DLIST_REMOVE(p->search, search);
		talloc_free(search);
	}

	return NT_STATUS_OK;
}

/* close a search */
static NTSTATUS svfs_search_close(struct ntvfs_module_context *ntvfs,
				  struct ntvfs_request *req, union smb_search_close *io)
{
	struct svfs_private *p = ntvfs->private_data;
	struct search_state *search;

	for (search=p->search; search; search = search->next) {
		if (search->handle == io->findclose.in.handle) break;
	}
	
	if (!search) {
		/* we didn't find the search handle */
		return NT_STATUS_FOOBAR;
	}

	DLIST_REMOVE(p->search, search);
	talloc_free(search);

	return NT_STATUS_OK;
}

/* SMBtrans - not used on file shares */
static NTSTATUS svfs_trans(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, struct smb_trans2 *trans2)
{
	return NT_STATUS_ACCESS_DENIED;
}


/*
  initialialise the POSIX disk backend, registering ourselves with the ntvfs subsystem
 */
NTSTATUS ntvfs_simple_init(void)
{
	NTSTATUS ret;
	struct ntvfs_ops ops;
	NTVFS_CURRENT_CRITICAL_SIZES(vers);

	ZERO_STRUCT(ops);

	/* fill in all the operations */
	ops.connect = svfs_connect;
	ops.disconnect = svfs_disconnect;
	ops.unlink = svfs_unlink;
	ops.chkpath = svfs_chkpath;
	ops.qpathinfo = svfs_qpathinfo;
	ops.setpathinfo = svfs_setpathinfo;
	ops.open = svfs_open;
	ops.mkdir = svfs_mkdir;
	ops.rmdir = svfs_rmdir;
	ops.rename = svfs_rename;
	ops.copy = svfs_copy;
	ops.ioctl = svfs_ioctl;
	ops.read = svfs_read;
	ops.write = svfs_write;
	ops.seek = svfs_seek;
	ops.flush = svfs_flush;	
	ops.close = svfs_close;
	ops.exit = svfs_exit;
	ops.lock = svfs_lock;
	ops.setfileinfo = svfs_setfileinfo;
	ops.qfileinfo = svfs_qfileinfo;
	ops.fsinfo = svfs_fsinfo;
	ops.lpq = svfs_lpq;
	ops.search_first = svfs_search_first;
	ops.search_next = svfs_search_next;
	ops.search_close = svfs_search_close;
	ops.trans = svfs_trans;
	ops.logoff = svfs_logoff;
	ops.async_setup = svfs_async_setup;
	ops.cancel = svfs_cancel;

	/* register ourselves with the NTVFS subsystem. We register
	   under names 'simple'
	*/

	ops.type = NTVFS_DISK;
	ops.name = "simple";
	ret = ntvfs_register(&ops, &vers);

	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register simple backend with name: %s!\n",
			 ops.name));
	}

	return ret;
}
