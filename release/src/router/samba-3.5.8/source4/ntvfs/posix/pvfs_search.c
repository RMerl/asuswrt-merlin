/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - directory search functions

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
#include "system/time.h"
#include "librpc/gen_ndr/security.h"
#include "smbd/service_stream.h"
#include "lib/events/events.h"
#include "../lib/util/dlinklist.h"

/* place a reasonable limit on old-style searches as clients tend to
   not send search close requests */
#define MAX_OLD_SEARCHES 2000
#define MAX_SEARCH_HANDLES (UINT16_MAX - 1)
#define INVALID_SEARCH_HANDLE UINT16_MAX

/*
  destroy an open search
*/
static int pvfs_search_destructor(struct pvfs_search_state *search)
{
	DLIST_REMOVE(search->pvfs->search.list, search);
	idr_remove(search->pvfs->search.idtree, search->handle);
	return 0;
}

/*
  called when a search timer goes off
*/
static void pvfs_search_timer(struct tevent_context *ev, struct tevent_timer *te, 
				      struct timeval t, void *ptr)
{
	struct pvfs_search_state *search = talloc_get_type(ptr, struct pvfs_search_state);
	talloc_free(search);
}

/*
  setup a timer to destroy a open search after a inactivity period
*/
static void pvfs_search_setup_timer(struct pvfs_search_state *search)
{
	struct tevent_context *ev = search->pvfs->ntvfs->ctx->event_ctx;
	if (search->handle == INVALID_SEARCH_HANDLE) return;
	talloc_free(search->te);
	search->te = event_add_timed(ev, search, 
				     timeval_current_ofs(search->pvfs->search.inactivity_time, 0), 
				     pvfs_search_timer, search);
}

/*
  fill in a single search result for a given info level
*/
static NTSTATUS fill_search_info(struct pvfs_state *pvfs,
				 enum smb_search_data_level level,
				 const char *unix_path,
				 const char *fname, 
				 struct pvfs_search_state *search,
				 off_t dir_offset,
				 union smb_search_data *file)
{
	struct pvfs_filename *name;
	NTSTATUS status;
	const char *shortname;
	uint32_t dir_index = (uint32_t)dir_offset; /* truncated - see the code 
						      in pvfs_list_seek_ofs() for 
						      how we cope with this */

	status = pvfs_resolve_partial(pvfs, file, unix_path, fname, 0, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_match_attrib(pvfs, name, search->search_attrib, search->must_attrib);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (level) {
	case RAW_SEARCH_DATA_SEARCH:
		shortname = pvfs_short_name(pvfs, name, name);
		file->search.attrib           = name->dos.attrib;
		file->search.write_time       = nt_time_to_unix(name->dos.write_time);
		file->search.size             = name->st.st_size;
		file->search.name             = shortname;
		file->search.id.reserved      = search->handle >> 8;
		memset(file->search.id.name, ' ', sizeof(file->search.id.name));
		memcpy(file->search.id.name, shortname, 
		       MIN(strlen(shortname)+1, sizeof(file->search.id.name)));
		file->search.id.handle        = search->handle & 0xFF;
		file->search.id.server_cookie = dir_index;
		file->search.id.client_cookie = 0;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_STANDARD:
		file->standard.resume_key   = dir_index;
		file->standard.create_time  = nt_time_to_unix(name->dos.create_time);
		file->standard.access_time  = nt_time_to_unix(name->dos.access_time);
		file->standard.write_time   = nt_time_to_unix(name->dos.write_time);
		file->standard.size         = name->st.st_size;
		file->standard.alloc_size   = name->dos.alloc_size;
		file->standard.attrib       = name->dos.attrib;
		file->standard.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_EA_SIZE:
		file->ea_size.resume_key   = dir_index;
		file->ea_size.create_time  = nt_time_to_unix(name->dos.create_time);
		file->ea_size.access_time  = nt_time_to_unix(name->dos.access_time);
		file->ea_size.write_time   = nt_time_to_unix(name->dos.write_time);
		file->ea_size.size         = name->st.st_size;
		file->ea_size.alloc_size   = name->dos.alloc_size;
		file->ea_size.attrib       = name->dos.attrib;
		file->ea_size.ea_size      = name->dos.ea_size;
		file->ea_size.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_EA_LIST:
		file->ea_list.resume_key   = dir_index;
		file->ea_list.create_time  = nt_time_to_unix(name->dos.create_time);
		file->ea_list.access_time  = nt_time_to_unix(name->dos.access_time);
		file->ea_list.write_time   = nt_time_to_unix(name->dos.write_time);
		file->ea_list.size         = name->st.st_size;
		file->ea_list.alloc_size   = name->dos.alloc_size;
		file->ea_list.attrib       = name->dos.attrib;
		file->ea_list.name.s       = fname;
		return pvfs_query_ea_list(pvfs, file, name, -1, 
					  search->num_ea_names,
					  search->ea_names,
					  &file->ea_list.eas);

	case RAW_SEARCH_DATA_DIRECTORY_INFO:
		file->directory_info.file_index   = dir_index;
		file->directory_info.create_time  = name->dos.create_time;
		file->directory_info.access_time  = name->dos.access_time;
		file->directory_info.write_time   = name->dos.write_time;
		file->directory_info.change_time  = name->dos.change_time;
		file->directory_info.size         = name->st.st_size;
		file->directory_info.alloc_size   = name->dos.alloc_size;
		file->directory_info.attrib       = name->dos.attrib;
		file->directory_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_FULL_DIRECTORY_INFO:
		file->full_directory_info.file_index   = dir_index;
		file->full_directory_info.create_time  = name->dos.create_time;
		file->full_directory_info.access_time  = name->dos.access_time;
		file->full_directory_info.write_time   = name->dos.write_time;
		file->full_directory_info.change_time  = name->dos.change_time;
		file->full_directory_info.size         = name->st.st_size;
		file->full_directory_info.alloc_size   = name->dos.alloc_size;
		file->full_directory_info.attrib       = name->dos.attrib;
		file->full_directory_info.ea_size      = name->dos.ea_size;
		file->full_directory_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_NAME_INFO:
		file->name_info.file_index   = dir_index;
		file->name_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO:
		file->both_directory_info.file_index   = dir_index;
		file->both_directory_info.create_time  = name->dos.create_time;
		file->both_directory_info.access_time  = name->dos.access_time;
		file->both_directory_info.write_time   = name->dos.write_time;
		file->both_directory_info.change_time  = name->dos.change_time;
		file->both_directory_info.size         = name->st.st_size;
		file->both_directory_info.alloc_size   = name->dos.alloc_size;
		file->both_directory_info.attrib       = name->dos.attrib;
		file->both_directory_info.ea_size      = name->dos.ea_size;
		file->both_directory_info.short_name.s = pvfs_short_name(pvfs, file, name);
		file->both_directory_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO:
		file->id_full_directory_info.file_index   = dir_index;
		file->id_full_directory_info.create_time  = name->dos.create_time;
		file->id_full_directory_info.access_time  = name->dos.access_time;
		file->id_full_directory_info.write_time   = name->dos.write_time;
		file->id_full_directory_info.change_time  = name->dos.change_time;
		file->id_full_directory_info.size         = name->st.st_size;
		file->id_full_directory_info.alloc_size   = name->dos.alloc_size;
		file->id_full_directory_info.attrib       = name->dos.attrib;
		file->id_full_directory_info.ea_size      = name->dos.ea_size;
		file->id_full_directory_info.file_id      = name->dos.file_id;
		file->id_full_directory_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO:
		file->id_both_directory_info.file_index   = dir_index;
		file->id_both_directory_info.create_time  = name->dos.create_time;
		file->id_both_directory_info.access_time  = name->dos.access_time;
		file->id_both_directory_info.write_time   = name->dos.write_time;
		file->id_both_directory_info.change_time  = name->dos.change_time;
		file->id_both_directory_info.size         = name->st.st_size;
		file->id_both_directory_info.alloc_size   = name->dos.alloc_size;
		file->id_both_directory_info.attrib       = name->dos.attrib;
		file->id_both_directory_info.ea_size      = name->dos.ea_size;
		file->id_both_directory_info.file_id      = name->dos.file_id;
		file->id_both_directory_info.short_name.s = pvfs_short_name(pvfs, file, name);
		file->id_both_directory_info.name.s       = fname;
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_GENERIC:
		break;
	}

	return NT_STATUS_INVALID_LEVEL;
}


/*
  the search fill loop
*/
static NTSTATUS pvfs_search_fill(struct pvfs_state *pvfs, TALLOC_CTX *mem_ctx, 
				 uint_t max_count, 
				 struct pvfs_search_state *search,
				 enum smb_search_data_level level,
				 uint_t *reply_count,
				 void *search_private, 
				 bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_dir *dir = search->dir;
	NTSTATUS status;

	*reply_count = 0;

	if (max_count == 0) {
		max_count = 1;
	}

	while ((*reply_count) < max_count) {
		union smb_search_data *file;
		const char *name;
		off_t ofs = search->current_index;

		name = pvfs_list_next(dir, &search->current_index);
		if (name == NULL) break;

		file = talloc(mem_ctx, union smb_search_data);
		if (!file) {
			return NT_STATUS_NO_MEMORY;
		}

		status = fill_search_info(pvfs, level, 
					  pvfs_list_unix_path(dir), name, 
					  search, search->current_index, file);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(file);
			continue;
		}

		if (!callback(search_private, file)) {
			talloc_free(file);
			search->current_index = ofs;
			break;
		}

		(*reply_count)++;
		talloc_free(file);
	}

	pvfs_search_setup_timer(search);

	return NT_STATUS_OK;
}

/*
  we've run out of search handles - cleanup those that the client forgot
  to close
*/
static void pvfs_search_cleanup(struct pvfs_state *pvfs)
{
	int i;
	time_t t = time(NULL);

	for (i=0;i<MAX_OLD_SEARCHES;i++) {
		struct pvfs_search_state *search;
		void *p = idr_find(pvfs->search.idtree, i);

		if (p == NULL) return;

		search = talloc_get_type(p, struct pvfs_search_state);
		if (pvfs_list_eos(search->dir, search->current_index) &&
		    search->last_used != 0 &&
		    t > search->last_used + 30) {
			/* its almost certainly been forgotten
			 about */
			talloc_free(search);
		}
	}
}


/* 
   list files in a directory matching a wildcard pattern - old SMBsearch interface
*/
static NTSTATUS pvfs_search_first_old(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req, union smb_search_first *io, 
				      void *search_private, 
				      bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_dir *dir;
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_search_state *search;
	uint_t reply_count;
	uint16_t search_attrib;
	const char *pattern;
	NTSTATUS status;
	struct pvfs_filename *name;
	int id;

	search_attrib = io->search_first.in.search_attrib;
	pattern       = io->search_first.in.pattern;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, pattern, PVFS_RESOLVE_WILDCARD, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!name->has_wildcard && !name->exists) {
		return STATUS_NO_MORE_FILES;
	}

	status = pvfs_access_check_parent(pvfs, req, name, SEC_DIR_TRAVERSE | SEC_DIR_LIST);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* we initially make search a child of the request, then if we
	   need to keep it long term we steal it for the private
	   structure */
	search = talloc(req, struct pvfs_search_state);
	if (!search) {
		return NT_STATUS_NO_MEMORY;
	}

	/* do the actual directory listing */
	status = pvfs_list_start(pvfs, name, search, &dir);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* we need to give a handle back to the client so it
	   can continue a search */
	id = idr_get_new(pvfs->search.idtree, search, MAX_OLD_SEARCHES);
	if (id == -1) {
		pvfs_search_cleanup(pvfs);
		id = idr_get_new(pvfs->search.idtree, search, MAX_OLD_SEARCHES);
	}
	if (id == -1) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	search->pvfs = pvfs;
	search->handle = id;
	search->dir = dir;
	search->current_index = 0;
	search->search_attrib = search_attrib & 0xFF;
	search->must_attrib = (search_attrib>>8) & 0xFF;
	search->last_used = time(NULL);
	search->te = NULL;

	DLIST_ADD(pvfs->search.list, search);

	talloc_set_destructor(search, pvfs_search_destructor);

	status = pvfs_search_fill(pvfs, req, io->search_first.in.max_count, search, io->generic.data_level,
				  &reply_count, search_private, callback);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	io->search_first.out.count = reply_count;

	/* not matching any entries is an error */
	if (reply_count == 0) {
		return STATUS_NO_MORE_FILES;
	}

	talloc_steal(pvfs, search);

	return NT_STATUS_OK;
}

/* continue a old style search */
static NTSTATUS pvfs_search_next_old(struct ntvfs_module_context *ntvfs,
				     struct ntvfs_request *req, union smb_search_next *io, 
				     void *search_private, 
				     bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	void *p;
	struct pvfs_search_state *search;
	struct pvfs_dir *dir;
	uint_t reply_count, max_count;
	uint16_t handle;
	NTSTATUS status;

	handle    = io->search_next.in.id.handle | (io->search_next.in.id.reserved<<8);
	max_count = io->search_next.in.max_count;

	p = idr_find(pvfs->search.idtree, handle);
	if (p == NULL) {
		/* we didn't find the search handle */
		return NT_STATUS_INVALID_HANDLE;
	}

	search = talloc_get_type(p, struct pvfs_search_state);

	dir = search->dir;

	status = pvfs_list_seek_ofs(dir, io->search_next.in.id.server_cookie, 
				    &search->current_index);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	search->last_used = time(NULL);

	status = pvfs_search_fill(pvfs, req, max_count, search, io->generic.data_level,
				  &reply_count, search_private, callback);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	io->search_next.out.count = reply_count;

	/* not matching any entries means end of search */
	if (reply_count == 0) {
		talloc_free(search);
	}

	return NT_STATUS_OK;
}

/* 
   list files in a directory matching a wildcard pattern
*/
static NTSTATUS pvfs_search_first_trans2(struct ntvfs_module_context *ntvfs,
					 struct ntvfs_request *req, union smb_search_first *io, 
					 void *search_private, 
					 bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_dir *dir;
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_search_state *search;
	uint_t reply_count;
	uint16_t search_attrib, max_count;
	const char *pattern;
	NTSTATUS status;
	struct pvfs_filename *name;
	int id;

	search_attrib = io->t2ffirst.in.search_attrib;
	pattern       = io->t2ffirst.in.pattern;
	max_count     = io->t2ffirst.in.max_count;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, pattern, PVFS_RESOLVE_WILDCARD, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!name->has_wildcard && !name->exists) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	status = pvfs_access_check_parent(pvfs, req, name, SEC_DIR_TRAVERSE | SEC_DIR_LIST);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* we initially make search a child of the request, then if we
	   need to keep it long term we steal it for the private
	   structure */
	search = talloc(req, struct pvfs_search_state);
	if (!search) {
		return NT_STATUS_NO_MEMORY;
	}

	/* do the actual directory listing */
	status = pvfs_list_start(pvfs, name, search, &dir);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	id = idr_get_new(pvfs->search.idtree, search, MAX_SEARCH_HANDLES);
	if (id == -1) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	search->pvfs = pvfs;
	search->handle = id;
	search->dir = dir;
	search->current_index = 0;
	search->search_attrib = search_attrib;
	search->must_attrib = 0;
	search->last_used = 0;
	search->num_ea_names = io->t2ffirst.in.num_names;
	search->ea_names = io->t2ffirst.in.ea_names;
	search->te = NULL;

	DLIST_ADD(pvfs->search.list, search);
	talloc_set_destructor(search, pvfs_search_destructor);

	status = pvfs_search_fill(pvfs, req, max_count, search, io->generic.data_level,
				  &reply_count, search_private, callback);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* not matching any entries is an error */
	if (reply_count == 0) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	io->t2ffirst.out.count = reply_count;
	io->t2ffirst.out.handle = search->handle;
	io->t2ffirst.out.end_of_search = pvfs_list_eos(dir, search->current_index) ? 1 : 0;

	/* work out if we are going to keep the search state
	   and allow for a search continue */
	if ((io->t2ffirst.in.flags & FLAG_TRANS2_FIND_CLOSE) ||
	    ((io->t2ffirst.in.flags & FLAG_TRANS2_FIND_CLOSE_IF_END) && 
	     io->t2ffirst.out.end_of_search)) {
		talloc_free(search);
	} else {
		talloc_steal(pvfs, search);
	}

	return NT_STATUS_OK;
}

/* continue a search */
static NTSTATUS pvfs_search_next_trans2(struct ntvfs_module_context *ntvfs,
					struct ntvfs_request *req, union smb_search_next *io, 
					void *search_private, 
					bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	void *p;
	struct pvfs_search_state *search;
	struct pvfs_dir *dir;
	uint_t reply_count;
	uint16_t handle;
	NTSTATUS status;

	handle = io->t2fnext.in.handle;

	p = idr_find(pvfs->search.idtree, handle);
	if (p == NULL) {
		/* we didn't find the search handle */
		return NT_STATUS_INVALID_HANDLE;
	}

	search = talloc_get_type(p, struct pvfs_search_state);

	dir = search->dir;
	
	status = NT_STATUS_OK;

	/* work out what type of continuation is being used */
	if (io->t2fnext.in.last_name && *io->t2fnext.in.last_name) {
		status = pvfs_list_seek(dir, io->t2fnext.in.last_name, &search->current_index);
		if (!NT_STATUS_IS_OK(status) && io->t2fnext.in.resume_key) {
			status = pvfs_list_seek_ofs(dir, io->t2fnext.in.resume_key, 
						    &search->current_index);
		}
	} else if (!(io->t2fnext.in.flags & FLAG_TRANS2_FIND_CONTINUE)) {
		status = pvfs_list_seek_ofs(dir, io->t2fnext.in.resume_key, 
					    &search->current_index);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	search->num_ea_names = io->t2fnext.in.num_names;
	search->ea_names = io->t2fnext.in.ea_names;

	status = pvfs_search_fill(pvfs, req, io->t2fnext.in.max_count, search, io->generic.data_level,
				  &reply_count, search_private, callback);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	io->t2fnext.out.count = reply_count;
	io->t2fnext.out.end_of_search = pvfs_list_eos(dir, search->current_index) ? 1 : 0;

	/* work out if we are going to keep the search state */
	if ((io->t2fnext.in.flags & FLAG_TRANS2_FIND_CLOSE) ||
	    ((io->t2fnext.in.flags & FLAG_TRANS2_FIND_CLOSE_IF_END) && 
	     io->t2fnext.out.end_of_search)) {
		talloc_free(search);
	}

	return NT_STATUS_OK;
}

static NTSTATUS pvfs_search_first_smb2(struct ntvfs_module_context *ntvfs,
				       struct ntvfs_request *req, const struct smb2_find *io, 
				       void *search_private, 
				       bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_dir *dir;
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_search_state *search;
	uint_t reply_count;
	uint16_t max_count;
	const char *pattern;
	NTSTATUS status;
	struct pvfs_filename *name;
	struct pvfs_file *f;

	f = pvfs_find_fd(pvfs, req, io->in.file.ntvfs);
	if (!f) {
		return NT_STATUS_FILE_CLOSED;
	}

	/* its only valid for directories */
	if (f->handle->fd != -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!(f->access_mask & SEC_DIR_LIST)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (f->search) {
		talloc_free(f->search);
		f->search = NULL;
	}

	if (strequal(io->in.pattern, "")) {
		return NT_STATUS_OBJECT_NAME_INVALID;
	}
	if (strchr_m(io->in.pattern, '\\')) {
		return NT_STATUS_OBJECT_NAME_INVALID;
	}
	if (strchr_m(io->in.pattern, '/')) {
		return NT_STATUS_OBJECT_NAME_INVALID;
	}

	if (strequal("", f->handle->name->original_name)) {
		pattern = talloc_asprintf(req, "\\%s", io->in.pattern);
		NT_STATUS_HAVE_NO_MEMORY(pattern);
	} else {
		pattern = talloc_asprintf(req, "\\%s\\%s",
					  f->handle->name->original_name,
					  io->in.pattern);
		NT_STATUS_HAVE_NO_MEMORY(pattern);
	}

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, pattern, PVFS_RESOLVE_WILDCARD, &name);
	NT_STATUS_NOT_OK_RETURN(status);

	if (!name->has_wildcard && !name->exists) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	/* we initially make search a child of the request, then if we
	   need to keep it long term we steal it for the private
	   structure */
	search = talloc(req, struct pvfs_search_state);
	NT_STATUS_HAVE_NO_MEMORY(search);

	/* do the actual directory listing */
	status = pvfs_list_start(pvfs, name, search, &dir);
	NT_STATUS_NOT_OK_RETURN(status);

	search->pvfs		= pvfs;
	search->handle		= INVALID_SEARCH_HANDLE;
	search->dir		= dir;
	search->current_index	= 0;
	search->search_attrib	= 0x0000FFFF;
	search->must_attrib	= 0;
	search->last_used	= 0;
	search->num_ea_names	= 0;
	search->ea_names	= NULL;
	search->te		= NULL;

	if (io->in.continue_flags & SMB2_CONTINUE_FLAG_SINGLE) {
		max_count = 1;
	} else {
		max_count = UINT16_MAX;
	}

	status = pvfs_search_fill(pvfs, req, max_count, search, io->data_level,
				  &reply_count, search_private, callback);
	NT_STATUS_NOT_OK_RETURN(status);

	/* not matching any entries is an error */
	if (reply_count == 0) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	f->search = talloc_steal(f, search);

	return NT_STATUS_OK;
}

static NTSTATUS pvfs_search_next_smb2(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req, const struct smb2_find *io, 
				      void *search_private, 
				      bool (*callback)(void *, const union smb_search_data *))
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_search_state *search;
	uint_t reply_count;
	uint16_t max_count;
	NTSTATUS status;
	struct pvfs_file *f;

	f = pvfs_find_fd(pvfs, req, io->in.file.ntvfs);
	if (!f) {
		return NT_STATUS_FILE_CLOSED;
	}

	/* its only valid for directories */
	if (f->handle->fd != -1) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* if there's no search started on the dir handle, it's like a search_first */
	search = f->search;
	if (!search) {
		return pvfs_search_first_smb2(ntvfs, req, io, search_private, callback);
	}

	if (io->in.continue_flags & SMB2_CONTINUE_FLAG_RESTART) {
		search->current_index = 0;
	}

	if (io->in.continue_flags & SMB2_CONTINUE_FLAG_SINGLE) {
		max_count = 1;
	} else {
		max_count = UINT16_MAX;
	}

	status = pvfs_search_fill(pvfs, req, max_count, search, io->data_level,
				  &reply_count, search_private, callback);
	NT_STATUS_NOT_OK_RETURN(status);

	/* not matching any entries is an error */
	if (reply_count == 0) {
		return STATUS_NO_MORE_FILES;
	}

	return NT_STATUS_OK;
}

/* 
   list files in a directory matching a wildcard pattern
*/
NTSTATUS pvfs_search_first(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_search_first *io, 
			   void *search_private, 
			   bool (*callback)(void *, const union smb_search_data *))
{
	switch (io->generic.level) {
	case RAW_SEARCH_SEARCH:
	case RAW_SEARCH_FFIRST:
	case RAW_SEARCH_FUNIQUE:
		return pvfs_search_first_old(ntvfs, req, io, search_private, callback);

	case RAW_SEARCH_TRANS2:
		return pvfs_search_first_trans2(ntvfs, req, io, search_private, callback);

	case RAW_SEARCH_SMB2:
		return pvfs_search_first_smb2(ntvfs, req, &io->smb2, search_private, callback);
	}

	return NT_STATUS_INVALID_LEVEL;
}

/* continue a search */
NTSTATUS pvfs_search_next(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_search_next *io, 
			  void *search_private, 
			  bool (*callback)(void *, const union smb_search_data *))
{
	switch (io->generic.level) {
	case RAW_SEARCH_SEARCH:
	case RAW_SEARCH_FFIRST:
		return pvfs_search_next_old(ntvfs, req, io, search_private, callback);

	case RAW_SEARCH_FUNIQUE:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_SEARCH_TRANS2:
		return pvfs_search_next_trans2(ntvfs, req, io, search_private, callback);

	case RAW_SEARCH_SMB2:
		return pvfs_search_next_smb2(ntvfs, req, &io->smb2, search_private, callback);
	}

	return NT_STATUS_INVALID_LEVEL;
}


/* close a search */
NTSTATUS pvfs_search_close(struct ntvfs_module_context *ntvfs,
			   struct ntvfs_request *req, union smb_search_close *io)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	void *p;
	struct pvfs_search_state *search;
	uint16_t handle = INVALID_SEARCH_HANDLE;

	switch (io->generic.level) {
	case RAW_FINDCLOSE_GENERIC:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_FINDCLOSE_FCLOSE:
		handle = io->fclose.in.id.handle;
		break;

	case RAW_FINDCLOSE_FINDCLOSE:
		handle = io->findclose.in.handle;
		break;
	}

	p = idr_find(pvfs->search.idtree, handle);
	if (p == NULL) {
		/* we didn't find the search handle */
		return NT_STATUS_INVALID_HANDLE;
	}

	search = talloc_get_type(p, struct pvfs_search_state);

	talloc_free(search);

	return NT_STATUS_OK;
}

