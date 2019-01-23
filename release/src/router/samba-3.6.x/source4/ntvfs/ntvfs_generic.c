/* 
   Unix SMB/CIFS implementation.

   NTVFS generic level mapping code

   Copyright (C) Andrew Tridgell 2003-2004

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
  this implements mappings between info levels for NTVFS backend calls

  the idea is that each of these functions implements one of the NTVFS
  backend calls in terms of the 'generic' call. All backends that use
  these functions must supply the generic call, but can if it wants to
  also implement other levels if the need arises

  this allows backend writers to only implement one variant of each
  call unless they need fine grained control of the calls.
*/

#include "includes.h"
#include "ntvfs/ntvfs.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

/* a second stage function converts from the out parameters of the generic
   call onto the out parameters of the specific call made */
typedef NTSTATUS (*second_stage_t)(struct ntvfs_module_context *,
				   struct ntvfs_request *,
				   void *, void *, NTSTATUS);

/* 
   this structure holds the async state for pending mapped async calls
*/
struct ntvfs_map_async {
	struct ntvfs_module_context *ntvfs;
	void *io, *io2;
	second_stage_t fn;
};

/*
  this is a async wrapper, called from the backend when it has completed
  a function that it has decided to reply to in an async fashion
*/
static void ntvfs_map_async_send(struct ntvfs_request *req)
{
	struct ntvfs_map_async *m = talloc_get_type(req->async_states->private_data,
				    struct ntvfs_map_async);

	ntvfs_async_state_pop(req);

	/* call the _finish function setup in ntvfs_map_async_setup() */
	req->async_states->status = m->fn(m->ntvfs, req, m->io, m->io2, req->async_states->status);

	/* call the send function from the next module up */
	req->async_states->send_fn(req);
}

/*
  prepare for calling a ntvfs backend with async support
  io is the original call structure
  io2 is the new call structure for the mapped call
  fn is a second stage function for processing the out arguments
*/
static NTSTATUS ntvfs_map_async_setup(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      void *io, void *io2,
				      second_stage_t fn)
{
	struct ntvfs_map_async *m;
	m = talloc(req, struct ntvfs_map_async);
	if (m == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	m->ntvfs = ntvfs;
	m->io = io;
	m->io2 = io2;
	m->fn = fn;
	return ntvfs_async_state_push(ntvfs, req, m, ntvfs_map_async_send);
}

/*
  called when first stage processing is complete. 
*/	
static NTSTATUS ntvfs_map_async_finish(struct ntvfs_request *req, NTSTATUS status)
{
	struct ntvfs_map_async *m;

	/* if the backend has decided to reply in an async fashion then
	   we don't need to do any work here */
	if (req->async_states->state & NTVFS_ASYNC_STATE_ASYNC) {
		return status;
	}

	/* the backend is replying immediately. call the 2nd stage function after popping our local
	   async state */
	m = talloc_get_type(req->async_states->private_data,
			    struct ntvfs_map_async);

	ntvfs_async_state_pop(req);

	return m->fn(m->ntvfs, req, m->io, m->io2, status);
}

/*
  see if a filename ends in EXE COM DLL or SYM. This is needed for the
  DENY_DOS mapping for OpenX
*/
bool is_exe_filename(const char *fname)
{
	char *p;
	p = strrchr(fname, '.');
	if (!p) {
		return false;
	}
	p++;
	if (strcasecmp(p, "EXE") == 0 ||
	    strcasecmp(p, "COM") == 0 ||
	    strcasecmp(p, "DLL") == 0 ||
	    strcasecmp(p, "SYM") == 0) {
		return true;
	}
	return false;
}


/* 
   NTVFS openx to ntcreatex mapper
*/
static NTSTATUS ntvfs_map_open_finish(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req, 
				      union smb_open *io, 
				      union smb_open *io2, 
				      NTSTATUS status)
{
	time_t write_time = 0;
	uint32_t set_size = 0;
	union smb_setfileinfo *sf;
	unsigned int state;

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	switch (io->generic.level) {
	case RAW_OPEN_OPEN:
		io->openold.out.file.ntvfs = io2->generic.out.file.ntvfs;
		io->openold.out.attrib     = io2->generic.out.attrib;
		io->openold.out.write_time = nt_time_to_unix(io2->generic.out.write_time);
		io->openold.out.size       = io2->generic.out.size;
		io->openold.out.rmode      = io->openold.in.open_mode;
		break;

	case RAW_OPEN_OPENX:
		io->openx.out.file.ntvfs  = io2->generic.out.file.ntvfs;
		io->openx.out.attrib      = io2->generic.out.attrib;
		io->openx.out.write_time  = nt_time_to_unix(io2->generic.out.write_time);
		io->openx.out.size        = io2->generic.out.size;
		io->openx.out.access      = (io->openx.in.open_mode & OPENX_MODE_ACCESS_MASK);
		io->openx.out.ftype       = 0;
		io->openx.out.devstate    = 0;
		io->openx.out.action      = io2->generic.out.create_action;
		io->openx.out.unique_fid  = 0;
		io->openx.out.access_mask = SEC_STD_ALL;
		io->openx.out.unknown     = 0;
		
		/* we need to extend the file to the requested size if
		   it was newly created */
		if (io2->generic.out.create_action == NTCREATEX_ACTION_CREATED) {
			set_size = io->openx.in.size;
		}
		break;

	case RAW_OPEN_T2OPEN:
		io->t2open.out.file.ntvfs  = io2->generic.out.file.ntvfs;
		io->t2open.out.attrib      = io2->generic.out.attrib;
		io->t2open.out.write_time  = nt_time_to_unix(io2->generic.out.write_time);
		io->t2open.out.size        = io2->generic.out.size;
		io->t2open.out.access      = io->t2open.in.open_mode;
		io->t2open.out.ftype       = 0;
		io->t2open.out.devstate    = 0;
		io->t2open.out.action      = io2->generic.out.create_action;
		io->t2open.out.file_id      = 0;
		break;

	case RAW_OPEN_MKNEW:
	case RAW_OPEN_CREATE:
		io->mknew.out.file.ntvfs= io2->generic.out.file.ntvfs;
		write_time		= io->mknew.in.write_time;
		break;

	case RAW_OPEN_CTEMP:
		io->ctemp.out.file.ntvfs= io2->generic.out.file.ntvfs;
		io->ctemp.out.name 	= talloc_strdup(req, io2->generic.in.fname + 
							strlen(io->ctemp.in.directory) + 1);
		NT_STATUS_HAVE_NO_MEMORY(io->ctemp.out.name);
		break;

	case RAW_OPEN_SMB2:
		ZERO_STRUCT(io->smb2.out);
		io->smb2.out.file.ntvfs		= io2->generic.out.file.ntvfs;
		switch (io2->generic.out.oplock_level) {
		case BATCH_OPLOCK_RETURN:
			io->smb2.out.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
			break;
		case EXCLUSIVE_OPLOCK_RETURN:
			io->smb2.out.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
			break;
		case LEVEL_II_OPLOCK_RETURN:
			io->smb2.out.oplock_level = SMB2_OPLOCK_LEVEL_II;
			break;
		default:
			io->smb2.out.oplock_level = SMB2_OPLOCK_LEVEL_NONE;
			break;
		}
		io->smb2.out.reserved		= 0;
		io->smb2.out.create_action	= io2->generic.out.create_action;
		io->smb2.out.create_time	= io2->generic.out.create_time;
		io->smb2.out.access_time	= io2->generic.out.access_time;
		io->smb2.out.write_time		= io2->generic.out.write_time;
		io->smb2.out.change_time	= io2->generic.out.change_time;
		io->smb2.out.alloc_size		= io2->generic.out.alloc_size;
		io->smb2.out.size		= io2->generic.out.size;
		io->smb2.out.file_attr		= io2->generic.out.attrib;
		io->smb2.out.reserved2		= 0;
		io->smb2.out.maximal_access     = io2->generic.out.maximal_access;
		break;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	/* doing a secondary request async is more trouble than its
	   worth */
	state = req->async_states->state;
	req->async_states->state &= ~NTVFS_ASYNC_STATE_MAY_ASYNC;

	if (write_time != 0) {
		sf = talloc(req, union smb_setfileinfo);
		NT_STATUS_HAVE_NO_MEMORY(sf);
		sf->generic.level           = RAW_SFILEINFO_STANDARD;
		sf->generic.in.file.ntvfs   = io2->generic.out.file.ntvfs;
		sf->standard.in.create_time = 0;
		sf->standard.in.write_time  = write_time;
		sf->standard.in.access_time = 0;
		status = ntvfs->ops->setfileinfo(ntvfs, req, sf);
	}

	if (set_size != 0) {
		sf = talloc(req, union smb_setfileinfo);			
		NT_STATUS_HAVE_NO_MEMORY(sf);
		sf->generic.level            = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
		sf->generic.in.file.ntvfs    = io2->generic.out.file.ntvfs;
		sf->end_of_file_info.in.size = set_size;
		status = ntvfs->ops->setfileinfo(ntvfs, req, sf);
		if (NT_STATUS_IS_OK(status)) {
			io->openx.out.size = io->openx.in.size;
		}
	}

	req->async_states->state = state;

	return NT_STATUS_OK;
}

/*
  the core of the mapping between openx style parameters and ntcreatex 
  parameters
*/
static NTSTATUS map_openx_open(uint16_t flags, uint16_t open_mode, 
			       uint16_t open_func, const char *fname,
			       union smb_open *io2)
{
	io2->generic.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io2->generic.in.private_flags = 0;

	if (flags & OPENX_FLAGS_REQUEST_OPLOCK) {
		io2->generic.in.flags |= NTCREATEX_FLAGS_REQUEST_OPLOCK;
	}
	if (flags & OPENX_FLAGS_REQUEST_BATCH_OPLOCK) {
		io2->generic.in.flags |= NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	}

	switch (open_mode & OPENX_MODE_ACCESS_MASK) {
	case OPENX_MODE_ACCESS_READ:
	case OPENX_MODE_ACCESS_EXEC:
		io2->generic.in.access_mask = SEC_RIGHTS_FILE_READ;
		break;
	case OPENX_MODE_ACCESS_WRITE:
		io2->generic.in.access_mask = SEC_RIGHTS_FILE_WRITE;
		break;
	case OPENX_MODE_ACCESS_RDWR:
	case OPENX_MODE_ACCESS_FCB:
		io2->generic.in.access_mask = 
			SEC_RIGHTS_FILE_READ | 
			SEC_RIGHTS_FILE_WRITE;
		break;
	default:
		return NT_STATUS_DOS(ERRDOS, ERRbadaccess);
	}

	switch (open_mode & OPENX_MODE_DENY_MASK) {
	case OPENX_MODE_DENY_READ:
		io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_WRITE;
		break;
	case OPENX_MODE_DENY_WRITE:
		io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_READ;
		break;
	case OPENX_MODE_DENY_ALL:
		io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
		break;
	case OPENX_MODE_DENY_NONE:
		io2->generic.in.share_access = 
			NTCREATEX_SHARE_ACCESS_READ | 
			NTCREATEX_SHARE_ACCESS_WRITE;
		break;
	case OPENX_MODE_DENY_DOS:
		/* DENY_DOS is quite strange - it depends on the filename! */
		io2->generic.in.private_flags |=
			NTCREATEX_OPTIONS_PRIVATE_DENY_DOS;
		if (is_exe_filename(fname)) {
			io2->generic.in.share_access = 
				NTCREATEX_SHARE_ACCESS_READ | 
				NTCREATEX_SHARE_ACCESS_WRITE;
		} else {
			if ((open_mode & OPENX_MODE_ACCESS_MASK) == OPENX_MODE_ACCESS_READ) {
				io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_READ;
			} else {
				io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
			}
		}
		break;
	case OPENX_MODE_DENY_FCB:
		io2->generic.in.private_flags |= NTCREATEX_OPTIONS_PRIVATE_DENY_FCB;
		io2->generic.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
		break;
	default:
		return NT_STATUS_DOS(ERRDOS, ERRbadaccess);
	}

	switch (open_func) {
	case (OPENX_OPEN_FUNC_OPEN):
		io2->generic.in.open_disposition = NTCREATEX_DISP_OPEN;
		break;
	case (OPENX_OPEN_FUNC_TRUNC):
		io2->generic.in.open_disposition = NTCREATEX_DISP_OVERWRITE;
		break;
	case (OPENX_OPEN_FUNC_FAIL | OPENX_OPEN_FUNC_CREATE):
		io2->generic.in.open_disposition = NTCREATEX_DISP_CREATE;
		break;
	case (OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE):
		io2->generic.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
		break;
	case (OPENX_OPEN_FUNC_TRUNC | OPENX_OPEN_FUNC_CREATE):
		io2->generic.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
		break;			
	default:
		/* this one is very strange */
		if ((open_mode & OPENX_MODE_ACCESS_MASK) == OPENX_MODE_ACCESS_EXEC) {
			io2->generic.in.open_disposition = NTCREATEX_DISP_CREATE;
			break;
		}
		return NT_STATUS_DOS(ERRDOS, ERRbadaccess);
	}

	return NT_STATUS_OK;
}

/* 
   NTVFS open generic to any mapper
*/
NTSTATUS ntvfs_map_open(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req,
				 union smb_open *io)
{
	NTSTATUS status;
	union smb_open *io2;

	io2 = talloc_zero(req, union smb_open);
	if (io2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ntvfs_map_async_setup(ntvfs, req,
				       io, io2, 
				       (second_stage_t)ntvfs_map_open_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	io2->generic.level = RAW_OPEN_GENERIC;
		
	switch (io->generic.level) {
	case RAW_OPEN_OPENX:
		status = map_openx_open(io->openx.in.flags,
					io->openx.in.open_mode, 
					io->openx.in.open_func, 
					io->openx.in.fname,
					io2);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}
		
		io2->generic.in.file_attr = io->openx.in.file_attrs;
		io2->generic.in.fname = io->openx.in.fname;
		
		status = ntvfs->ops->open(ntvfs, req, io2);
		break;
		
		
	case RAW_OPEN_OPEN:
		status = map_openx_open(0,
					io->openold.in.open_mode, 
					OPENX_OPEN_FUNC_OPEN, 
					io->openold.in.fname,
					io2);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		io2->generic.in.file_attr = io->openold.in.search_attrs;
		io2->generic.in.fname = io->openold.in.fname;

		status = ntvfs->ops->open(ntvfs, req, io2);
		break;

	case RAW_OPEN_T2OPEN:
		io2->generic.level         = RAW_OPEN_NTTRANS_CREATE;

		if (io->t2open.in.open_func == 0) {
			status = NT_STATUS_OBJECT_NAME_COLLISION;
			goto done;
		}

		status = map_openx_open(io->t2open.in.flags,
					io->t2open.in.open_mode, 
					io->t2open.in.open_func, 
					io->t2open.in.fname,
					io2);
		if (!NT_STATUS_IS_OK(status)) {
			goto done;
		}

		io2->generic.in.file_attr        = io->t2open.in.file_attrs;
		io2->generic.in.fname            = io->t2open.in.fname;
		io2->generic.in.ea_list          = talloc(io2, struct smb_ea_list);
		io2->generic.in.ea_list->num_eas = io->t2open.in.num_eas;
		io2->generic.in.ea_list->eas     = io->t2open.in.eas;

		status = ntvfs->ops->open(ntvfs, req, io2);
		break;

	case RAW_OPEN_MKNEW:
		io2->generic.in.file_attr = io->mknew.in.attrib;
		io2->generic.in.fname = io->mknew.in.fname;
		io2->generic.in.open_disposition = NTCREATEX_DISP_CREATE;
		io2->generic.in.access_mask = 
			SEC_RIGHTS_FILE_READ |
			SEC_RIGHTS_FILE_WRITE;
		io2->generic.in.share_access = 
			NTCREATEX_SHARE_ACCESS_READ | 
			NTCREATEX_SHARE_ACCESS_WRITE;
		status = ntvfs->ops->open(ntvfs, req, io2);
		break;

	case RAW_OPEN_CREATE:
		io2->generic.in.file_attr = io->mknew.in.attrib;
		io2->generic.in.fname = io->mknew.in.fname;
		io2->generic.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
		io2->generic.in.access_mask = 
			SEC_RIGHTS_FILE_READ |
			SEC_RIGHTS_FILE_WRITE;
		io2->generic.in.share_access = 
			NTCREATEX_SHARE_ACCESS_READ | 
			NTCREATEX_SHARE_ACCESS_WRITE;
		status = ntvfs->ops->open(ntvfs, req, io2);
		break;

	case RAW_OPEN_CTEMP:
		io2->generic.in.file_attr = io->ctemp.in.attrib;
		io2->generic.in.fname = 
			talloc_asprintf(io2, "%s\\SRV%s", 
					io->ctemp.in.directory,
					generate_random_str_list(io2, 5, "0123456789"));
		io2->generic.in.open_disposition = NTCREATEX_DISP_CREATE;
		io2->generic.in.access_mask = 
			SEC_RIGHTS_FILE_READ |
			SEC_RIGHTS_FILE_WRITE;
		io2->generic.in.share_access = 
			NTCREATEX_SHARE_ACCESS_READ | 
			NTCREATEX_SHARE_ACCESS_WRITE;
		status = ntvfs->ops->open(ntvfs, req, io2);
		break;
	case RAW_OPEN_SMB2:
		switch (io->smb2.in.oplock_level) {
		case SMB2_OPLOCK_LEVEL_BATCH:
			io2->generic.in.flags = NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK |
						NTCREATEX_FLAGS_REQUEST_OPLOCK;
			break;
		case SMB2_OPLOCK_LEVEL_EXCLUSIVE:
			io2->generic.in.flags = NTCREATEX_FLAGS_REQUEST_OPLOCK;
			break;
		default:
			io2->generic.in.flags = 0;
			break;
		}
		io2->generic.in.root_fid.fnum	= 0;
		io2->generic.in.access_mask	= io->smb2.in.desired_access;
		io2->generic.in.alloc_size	= io->smb2.in.alloc_size;
		io2->generic.in.file_attr	= io->smb2.in.file_attributes;
		io2->generic.in.share_access	= io->smb2.in.share_access;
		io2->generic.in.open_disposition= io->smb2.in.create_disposition;
		io2->generic.in.create_options	= io->smb2.in.create_options;
		io2->generic.in.impersonation	= io->smb2.in.impersonation_level;
		io2->generic.in.security_flags	= 0;
		io2->generic.in.fname		= io->smb2.in.fname;
		io2->generic.in.sec_desc	= io->smb2.in.sec_desc;
		io2->generic.in.ea_list		= &io->smb2.in.eas;
		io2->generic.in.query_maximal_access = io->smb2.in.query_maximal_access; 
		io2->generic.in.private_flags	= 0;

		/* we don't support timewarp yet */
		if (io->smb2.in.timewarp != 0) {
			status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			break;
		}

		/* we need to check these bits before we check the private mask */
		if (io2->generic.in.create_options & SMB2_CREATE_OPTIONS_NOT_SUPPORTED_MASK) {
			DEBUG(2,(__location__ " create_options 0x%x not supported\n",
				 io2->generic.in.create_options));
			status = NT_STATUS_NOT_SUPPORTED;
			break;
		}

		/* TODO: find out why only SMB2 ignores these */
		io2->generic.in.create_options &= ~NTCREATEX_OPTIONS_SYNC_ALERT;
		io2->generic.in.create_options &= ~NTCREATEX_OPTIONS_ASYNC_ALERT;

		status = ntvfs->ops->open(ntvfs, req, io2);		
		break;

	default:
		status = NT_STATUS_INVALID_LEVEL;
		break;
	}
done:
	return ntvfs_map_async_finish(req, status);
}


/* 
   NTVFS any to fsinfo mapper
*/
static NTSTATUS ntvfs_map_fsinfo_finish(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      union smb_fsinfo *fs,
				      union smb_fsinfo *fs2,
				      NTSTATUS status)
{
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* and convert it to the required level */
	switch (fs->generic.level) {
	case RAW_QFS_GENERIC:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_QFS_DSKATTR: {
		/* map from generic to DSKATTR */
		unsigned int bpunit = 64;

		/* we need to scale the sizes to fit */
		for (bpunit=64; bpunit<0x10000; bpunit *= 2) {
			if (fs2->generic.out.blocks_total * (double)fs2->generic.out.block_size < bpunit * 512 * 65535.0) {
				break;
			}
		}

		fs->dskattr.out.blocks_per_unit = bpunit;
		fs->dskattr.out.block_size = 512;
		fs->dskattr.out.units_total = 
			(fs2->generic.out.blocks_total * (double)fs2->generic.out.block_size) / (bpunit * 512);
		fs->dskattr.out.units_free  = 
			(fs2->generic.out.blocks_free  * (double)fs2->generic.out.block_size) / (bpunit * 512);

		/* we must return a maximum of 2G to old DOS systems, or they get very confused */
		if (bpunit > 64 && req->ctx->protocol <= PROTOCOL_LANMAN2) {
			fs->dskattr.out.blocks_per_unit = 64;
			fs->dskattr.out.units_total = 0xFFFF;
			fs->dskattr.out.units_free = 0xFFFF;
		}
		return NT_STATUS_OK;
	}

	case RAW_QFS_ALLOCATION:
		fs->allocation.out.fs_id = fs2->generic.out.fs_id;
		fs->allocation.out.total_alloc_units = fs2->generic.out.blocks_total;
		fs->allocation.out.avail_alloc_units = fs2->generic.out.blocks_free;
		fs->allocation.out.sectors_per_unit = 1;
		fs->allocation.out.bytes_per_sector = fs2->generic.out.block_size;
		return NT_STATUS_OK;

	case RAW_QFS_VOLUME:
		fs->volume.out.serial_number = fs2->generic.out.serial_number;
		fs->volume.out.volume_name.s = fs2->generic.out.volume_name;
		return NT_STATUS_OK;

	case RAW_QFS_VOLUME_INFO:
	case RAW_QFS_VOLUME_INFORMATION:
		fs->volume_info.out.create_time = fs2->generic.out.create_time;
		fs->volume_info.out.serial_number = fs2->generic.out.serial_number;
		fs->volume_info.out.volume_name.s = fs2->generic.out.volume_name;
		return NT_STATUS_OK;

	case RAW_QFS_SIZE_INFO:
	case RAW_QFS_SIZE_INFORMATION:
		fs->size_info.out.total_alloc_units = fs2->generic.out.blocks_total;
		fs->size_info.out.avail_alloc_units = fs2->generic.out.blocks_free;
		fs->size_info.out.sectors_per_unit = 1;
		fs->size_info.out.bytes_per_sector = fs2->generic.out.block_size;
		return NT_STATUS_OK;

	case RAW_QFS_DEVICE_INFO:
	case RAW_QFS_DEVICE_INFORMATION:
		fs->device_info.out.device_type = fs2->generic.out.device_type;
		fs->device_info.out.characteristics = fs2->generic.out.device_characteristics;
		return NT_STATUS_OK;

	case RAW_QFS_ATTRIBUTE_INFO:
	case RAW_QFS_ATTRIBUTE_INFORMATION:
		fs->attribute_info.out.fs_attr = fs2->generic.out.fs_attr;
		fs->attribute_info.out.max_file_component_length = fs2->generic.out.max_file_component_length;
		fs->attribute_info.out.fs_type.s = fs2->generic.out.fs_type;
		return NT_STATUS_OK;

	case RAW_QFS_QUOTA_INFORMATION:
		ZERO_STRUCT(fs->quota_information.out.unknown);
		fs->quota_information.out.quota_soft = fs2->generic.out.quota_soft;
		fs->quota_information.out.quota_hard = fs2->generic.out.quota_hard;
		fs->quota_information.out.quota_flags = fs2->generic.out.quota_flags;
		return NT_STATUS_OK;

	case RAW_QFS_FULL_SIZE_INFORMATION:
		fs->full_size_information.out.total_alloc_units = fs2->generic.out.blocks_total;
		fs->full_size_information.out.call_avail_alloc_units = fs2->generic.out.blocks_free;
		fs->full_size_information.out.actual_avail_alloc_units = fs2->generic.out.blocks_free;
		fs->full_size_information.out.sectors_per_unit = 1;
		fs->full_size_information.out.bytes_per_sector = fs2->generic.out.block_size;
		return NT_STATUS_OK;

	case RAW_QFS_OBJECTID_INFORMATION:
		fs->objectid_information.out.guid = fs2->generic.out.guid;
		ZERO_STRUCT(fs->objectid_information.out.unknown);
		return NT_STATUS_OK;
	}


	return NT_STATUS_INVALID_LEVEL;
}

/*
   NTVFS fsinfo any to generic mapper
*/
NTSTATUS ntvfs_map_fsinfo(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req,
			  union smb_fsinfo *fs)
{
	NTSTATUS status;
	union smb_fsinfo *fs2;

	fs2 = talloc(req, union smb_fsinfo);
	if (fs2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (fs->generic.level == RAW_QFS_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	status = ntvfs_map_async_setup(ntvfs, req, fs, fs2,
				       (second_stage_t)ntvfs_map_fsinfo_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* ask the backend for the generic info */
	fs2->generic.level = RAW_QFS_GENERIC;

	status = ntvfs->ops->fsinfo(ntvfs, req, fs2);
	return ntvfs_map_async_finish(req, status);
}


/* 
   NTVFS fileinfo generic to any mapper
*/
NTSTATUS ntvfs_map_fileinfo(TALLOC_CTX *mem_ctx,
				     union smb_fileinfo *info, 
				     union smb_fileinfo *info2)
{
	int i;
	/* and convert it to the required level using results in info2 */
	switch (info->generic.level) {
		case RAW_FILEINFO_GENERIC:
		return NT_STATUS_INVALID_LEVEL;
	case RAW_FILEINFO_GETATTR:
		info->getattr.out.attrib = info2->generic.out.attrib & 0xff;
		info->getattr.out.size = info2->generic.out.size;
		info->getattr.out.write_time = nt_time_to_unix(info2->generic.out.write_time);
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_GETATTRE:
		info->getattre.out.attrib = info2->generic.out.attrib;
		info->getattre.out.size = info2->generic.out.size;
		info->getattre.out.write_time = nt_time_to_unix(info2->generic.out.write_time);
		info->getattre.out.create_time = nt_time_to_unix(info2->generic.out.create_time);
		info->getattre.out.access_time = nt_time_to_unix(info2->generic.out.access_time);
		info->getattre.out.alloc_size = info2->generic.out.alloc_size;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:
		info->network_open_information.out.create_time = info2->generic.out.create_time;
		info->network_open_information.out.access_time = info2->generic.out.access_time;
		info->network_open_information.out.write_time =  info2->generic.out.write_time;
		info->network_open_information.out.change_time = info2->generic.out.change_time;
		info->network_open_information.out.alloc_size = info2->generic.out.alloc_size;
		info->network_open_information.out.size = info2->generic.out.size;
		info->network_open_information.out.attrib = info2->generic.out.attrib;
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_INFO:
	case RAW_FILEINFO_ALL_INFORMATION:
		info->all_info.out.create_time = info2->generic.out.create_time;
		info->all_info.out.access_time = info2->generic.out.access_time;
		info->all_info.out.write_time =  info2->generic.out.write_time;
		info->all_info.out.change_time = info2->generic.out.change_time;
		info->all_info.out.attrib = info2->generic.out.attrib;
		info->all_info.out.alloc_size = info2->generic.out.alloc_size;
		info->all_info.out.size = info2->generic.out.size;
		info->all_info.out.nlink = info2->generic.out.nlink;
		info->all_info.out.delete_pending = info2->generic.out.delete_pending;
		info->all_info.out.directory = info2->generic.out.directory;
		info->all_info.out.ea_size = info2->generic.out.ea_size;
		info->all_info.out.fname.s = info2->generic.out.fname.s;
		info->all_info.out.fname.private_length = info2->generic.out.fname.private_length;
		return NT_STATUS_OK;

	case RAW_FILEINFO_BASIC_INFO:
	case RAW_FILEINFO_BASIC_INFORMATION:
		info->basic_info.out.create_time = info2->generic.out.create_time;
		info->basic_info.out.access_time = info2->generic.out.access_time;
		info->basic_info.out.write_time = info2->generic.out.write_time;
		info->basic_info.out.change_time = info2->generic.out.change_time;
		info->basic_info.out.attrib = info2->generic.out.attrib;
		return NT_STATUS_OK;

	case RAW_FILEINFO_STANDARD:
		info->standard.out.create_time = nt_time_to_unix(info2->generic.out.create_time);
		info->standard.out.access_time = nt_time_to_unix(info2->generic.out.access_time);
		info->standard.out.write_time = nt_time_to_unix(info2->generic.out.write_time);
		info->standard.out.size = info2->generic.out.size;
		info->standard.out.alloc_size = info2->generic.out.alloc_size;
		info->standard.out.attrib = info2->generic.out.attrib;
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_SIZE:
		info->ea_size.out.create_time = nt_time_to_unix(info2->generic.out.create_time);
		info->ea_size.out.access_time = nt_time_to_unix(info2->generic.out.access_time);
		info->ea_size.out.write_time = nt_time_to_unix(info2->generic.out.write_time);
		info->ea_size.out.size = info2->generic.out.size;
		info->ea_size.out.alloc_size = info2->generic.out.alloc_size;
		info->ea_size.out.attrib = info2->generic.out.attrib;
		info->ea_size.out.ea_size = info2->generic.out.ea_size;
		return NT_STATUS_OK;

	case RAW_FILEINFO_STANDARD_INFO:
	case RAW_FILEINFO_STANDARD_INFORMATION:
		info->standard_info.out.alloc_size = info2->generic.out.alloc_size;
		info->standard_info.out.size = info2->generic.out.size;
		info->standard_info.out.nlink = info2->generic.out.nlink;
		info->standard_info.out.delete_pending = info2->generic.out.delete_pending;
		info->standard_info.out.directory = info2->generic.out.directory;
		return NT_STATUS_OK;

	case RAW_FILEINFO_INTERNAL_INFORMATION:
		info->internal_information.out.file_id = info2->generic.out.file_id;
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_INFO:
	case RAW_FILEINFO_EA_INFORMATION:
		info->ea_info.out.ea_size = info2->generic.out.ea_size;
		return NT_STATUS_OK;

	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		info->attribute_tag_information.out.attrib = info2->generic.out.attrib;
		info->attribute_tag_information.out.reparse_tag = info2->generic.out.reparse_tag;
		return NT_STATUS_OK;

	case RAW_FILEINFO_STREAM_INFO:
	case RAW_FILEINFO_STREAM_INFORMATION:
		info->stream_info.out.num_streams = info2->generic.out.num_streams;
		if (info->stream_info.out.num_streams > 0) {
			info->stream_info.out.streams = 
				talloc_array(mem_ctx, 
					       struct stream_struct,
					       info->stream_info.out.num_streams);
			if (!info->stream_info.out.streams) {
				DEBUG(2,("ntvfs_map_fileinfo: no memory for %d streams\n",
					info->stream_info.out.num_streams));
				return NT_STATUS_NO_MEMORY;
			}
			for (i=0; i < info->stream_info.out.num_streams; i++) {
				info->stream_info.out.streams[i] = info2->generic.out.streams[i];
				info->stream_info.out.streams[i].stream_name.s = 
					talloc_strdup(info->stream_info.out.streams,
						      info2->generic.out.streams[i].stream_name.s);
				if (!info->stream_info.out.streams[i].stream_name.s) {
					DEBUG(2,("ntvfs_map_fileinfo: no memory for stream_name\n"));
					return NT_STATUS_NO_MEMORY;
				}
			}
		}
		return NT_STATUS_OK;

	case RAW_FILEINFO_NAME_INFO:
	case RAW_FILEINFO_NAME_INFORMATION:
		info->name_info.out.fname.s = talloc_strdup(mem_ctx, info2->generic.out.fname.s);
		NT_STATUS_HAVE_NO_MEMORY(info->name_info.out.fname.s);
		info->name_info.out.fname.private_length = info2->generic.out.fname.private_length;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_ALT_NAME_INFO:
	case RAW_FILEINFO_ALT_NAME_INFORMATION:
		info->alt_name_info.out.fname.s = talloc_strdup(mem_ctx, info2->generic.out.alt_fname.s);
		NT_STATUS_HAVE_NO_MEMORY(info->alt_name_info.out.fname.s);
		info->alt_name_info.out.fname.private_length = info2->generic.out.alt_fname.private_length;
		return NT_STATUS_OK;
	
	case RAW_FILEINFO_POSITION_INFORMATION:
		info->position_information.out.position = info2->generic.out.position;
		return NT_STATUS_OK;
	
	case RAW_FILEINFO_ALL_EAS:
		info->all_eas.out.num_eas = info2->generic.out.num_eas;
		if (info->all_eas.out.num_eas > 0) {
			info->all_eas.out.eas = talloc_array(mem_ctx, 
							       struct ea_struct,
							       info->all_eas.out.num_eas);
			if (!info->all_eas.out.eas) {
				DEBUG(2,("ntvfs_map_fileinfo: no memory for %d eas\n",
					info->all_eas.out.num_eas));
				return NT_STATUS_NO_MEMORY;
			}
			for (i = 0; i < info->all_eas.out.num_eas; i++) {
				info->all_eas.out.eas[i] = info2->generic.out.eas[i];
				info->all_eas.out.eas[i].name.s = 
					talloc_strdup(info->all_eas.out.eas,
						      info2->generic.out.eas[i].name.s);
				if (!info->all_eas.out.eas[i].name.s) {
					DEBUG(2,("ntvfs_map_fileinfo: no memory for stream_name\n"));
					return NT_STATUS_NO_MEMORY;
				}
				info->all_eas.out.eas[i].value.data = 
					(uint8_t *)talloc_memdup(info->all_eas.out.eas,
						info2->generic.out.eas[i].value.data,
						info2->generic.out.eas[i].value.length);
				if (!info->all_eas.out.eas[i].value.data) {
					DEBUG(2,("ntvfs_map_fileinfo: no memory for stream_name\n"));
					return NT_STATUS_NO_MEMORY;
				}
			}
		}
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_IS_NAME_VALID:
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_COMPRESSION_INFO:
	case RAW_FILEINFO_COMPRESSION_INFORMATION:
		info->compression_info.out.compressed_size = info2->generic.out.compressed_size;
		info->compression_info.out.format = info2->generic.out.format;
		info->compression_info.out.unit_shift = info2->generic.out.unit_shift;
		info->compression_info.out.chunk_shift = info2->generic.out.chunk_shift;
		info->compression_info.out.cluster_shift = info2->generic.out.cluster_shift;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_ACCESS_INFORMATION:
		info->access_information.out.access_flags = info2->generic.out.access_flags;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_MODE_INFORMATION:
		info->mode_information.out.mode = info2->generic.out.mode;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_ALIGNMENT_INFORMATION:
		info->alignment_information.out.alignment_requirement =
			info2->generic.out.alignment_requirement;
		return NT_STATUS_OK;
#if 0	
	case RAW_FILEINFO_UNIX_BASIC:
		info->unix_basic_info.out.end_of_file = info2->generic.out.end_of_file;
		info->unix_basic_info.out.num_bytes = info2->generic.out.size;
		info->unix_basic_info.out.status_change_time = info2->generic.out.change_time;
		info->unix_basic_info.out.access_time = info2->generic.out.access_time;
		info->unix_basic_info.out.change_time = info2->generic.out.change_time;
		info->unix_basic_info.out.uid = info2->generic.out.uid;
		info->unix_basic_info.out.gid = info2->generic.out.gid;
		info->unix_basic_info.out.file_type = info2->generic.out.file_type;
		info->unix_basic_info.out.dev_major = info2->generic.out.device;
		info->unix_basic_info.out.dev_minor = info2->generic.out.device;
		info->unix_basic_info.out.unique_id = info2->generic.out.inode;
		info->unix_basic_info.out.permissions = info2->generic.out.permissions;
		info->unix_basic_info.out.nlink = info2->generic.out.nlink;
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_UNIX_LINK:
		info->unix_link_info.out.link_dest = info2->generic.out.link_dest;
		return NT_STATUS_OK;
#endif
	}

	return NT_STATUS_INVALID_LEVEL;
}

/* 
   NTVFS any to fileinfo mapper
*/
static NTSTATUS ntvfs_map_qfileinfo_finish(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      union smb_fileinfo *info,
				      union smb_fileinfo *info2,
				      NTSTATUS status)
{
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return ntvfs_map_fileinfo(req, info, info2);
}

/*
   NTVFS fileinfo generic to any mapper
*/
NTSTATUS ntvfs_map_qfileinfo(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      union smb_fileinfo *info)
{
	NTSTATUS status;
	union smb_fileinfo *info2;

	info2 = talloc(req, union smb_fileinfo);
	if (info2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (info->generic.level == RAW_FILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	status = ntvfs_map_async_setup(ntvfs, req, info, info2,
				       (second_stage_t)ntvfs_map_qfileinfo_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* ask the backend for the generic info */
	info2->generic.level = RAW_FILEINFO_GENERIC;
	info2->generic.in.file.ntvfs= info->generic.in.file.ntvfs;

	status = ntvfs->ops->qfileinfo(ntvfs, req, info2);
	return ntvfs_map_async_finish(req, status);
}

/*
   NTVFS any to fileinfo mapper
*/
static NTSTATUS ntvfs_map_qpathinfo_finish(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      union smb_fileinfo *info,
				      union smb_fileinfo *info2,
				      NTSTATUS status)
{
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return ntvfs_map_fileinfo(req, info, info2);
}

/* 
   NTVFS pathinfo generic to any mapper
*/
NTSTATUS ntvfs_map_qpathinfo(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req,
				      union smb_fileinfo *info)
{
	NTSTATUS status;
	union smb_fileinfo *info2;

	info2 = talloc(req, union smb_fileinfo);
	if (info2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (info->generic.level == RAW_FILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	status = ntvfs_map_async_setup(ntvfs, req, info, info2,
				       (second_stage_t)ntvfs_map_qpathinfo_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* ask the backend for the generic info */
	info2->generic.level		= RAW_FILEINFO_GENERIC;
	info2->generic.in.file.path	= info->generic.in.file.path;

	status = ntvfs->ops->qpathinfo(ntvfs, req, info2);
	return ntvfs_map_async_finish(req, status);
}


/* 
   NTVFS lock generic to any mapper
*/
NTSTATUS ntvfs_map_lock(struct ntvfs_module_context *ntvfs,
			struct ntvfs_request *req,
			union smb_lock *lck)
{
	union smb_lock *lck2;
	struct smb_lock_entry *locks;

	lck2 = talloc(req, union smb_lock);
	if (lck2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	locks = talloc_array(lck2, struct smb_lock_entry, 1);
	if (locks == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (lck->generic.level) {
	case RAW_LOCK_LOCKX:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_LOCK_LOCK:
		lck2->generic.level = RAW_LOCK_GENERIC;
		lck2->generic.in.file.ntvfs= lck->lock.in.file.ntvfs;
		lck2->generic.in.mode = 0;
		lck2->generic.in.timeout = 0;
		lck2->generic.in.ulock_cnt = 0;
		lck2->generic.in.lock_cnt = 1;
		lck2->generic.in.locks = locks;
		locks->pid = req->smbpid;
		locks->offset = lck->lock.in.offset;
		locks->count = lck->lock.in.count;
		break;

	case RAW_LOCK_UNLOCK:
		lck2->generic.level = RAW_LOCK_GENERIC;
		lck2->generic.in.file.ntvfs= lck->unlock.in.file.ntvfs;
		lck2->generic.in.mode = 0;
		lck2->generic.in.timeout = 0;
		lck2->generic.in.ulock_cnt = 1;
		lck2->generic.in.lock_cnt = 0;
		lck2->generic.in.locks = locks;
		locks->pid = req->smbpid;
		locks->offset = lck->unlock.in.offset;
		locks->count = lck->unlock.in.count;
		break;

	case RAW_LOCK_SMB2: {
		/* this is only approximate! We need to change the
		   generic structure to fix this properly */
		int i;
		bool isunlock;
		if (lck->smb2.in.lock_count < 1) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		lck2->generic.level = RAW_LOCK_GENERIC;
		lck2->generic.in.file.ntvfs= lck->smb2.in.file.ntvfs;
		lck2->generic.in.timeout = UINT32_MAX;
		lck2->generic.in.mode = 0;
		lck2->generic.in.lock_cnt = 0;
		lck2->generic.in.ulock_cnt = 0;
		lck2->generic.in.locks = talloc_zero_array(lck2, struct smb_lock_entry, 
							   lck->smb2.in.lock_count);
		if (lck2->generic.in.locks == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		/* only the first lock gives the UNLOCK bit - see
		   MS-SMB2 3.3.5.14 */
		if (lck->smb2.in.locks[0].flags & SMB2_LOCK_FLAG_UNLOCK) {
			if (lck->smb2.in.locks[0].flags & SMB2_LOCK_FLAG_FAIL_IMMEDIATELY) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			lck2->generic.in.ulock_cnt = lck->smb2.in.lock_count;
			isunlock = true;
		} else {
			lck2->generic.in.lock_cnt = lck->smb2.in.lock_count;
			isunlock = false;
		}
		for (i=0;i<lck->smb2.in.lock_count;i++) {
			if (!isunlock &&
			    lck->smb2.in.locks[i].flags == SMB2_LOCK_FLAG_NONE) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			if (lck->smb2.in.locks[i].flags & ~SMB2_LOCK_FLAG_ALL_MASK) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			if (isunlock && 
			    (lck->smb2.in.locks[i].flags & 
			     (SMB2_LOCK_FLAG_SHARED|SMB2_LOCK_FLAG_EXCLUSIVE))) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			if (!isunlock && 
			    (lck->smb2.in.locks[i].flags & SMB2_LOCK_FLAG_UNLOCK)) {
				return NT_STATUS_INVALID_PARAMETER;
			}
			lck2->generic.in.locks[i].pid    = req->smbpid;
			lck2->generic.in.locks[i].offset = lck->smb2.in.locks[i].offset;
			lck2->generic.in.locks[i].count  = lck->smb2.in.locks[i].length;
			if (!(lck->smb2.in.locks[i].flags & SMB2_LOCK_FLAG_EXCLUSIVE)) {
				lck2->generic.in.mode = LOCKING_ANDX_SHARED_LOCK;
			}
			if (lck->smb2.in.locks[i].flags & SMB2_LOCK_FLAG_FAIL_IMMEDIATELY) {
				lck2->generic.in.timeout = 0;
			}
		}
		/* initialize output value */
		lck->smb2.out.reserved = 0;
		break;
	}

	case RAW_LOCK_SMB2_BREAK:
		lck2->generic.level		= RAW_LOCK_GENERIC;
		lck2->generic.in.file.ntvfs	= lck->smb2_break.in.file.ntvfs;
		lck2->generic.in.mode		= LOCKING_ANDX_OPLOCK_RELEASE |
						  ((lck->smb2_break.in.oplock_level << 8) & 0xFF00);
		lck2->generic.in.timeout	= 0;
		lck2->generic.in.ulock_cnt	= 0;
		lck2->generic.in.lock_cnt	= 0;
		lck2->generic.in.locks		= NULL;

		/* initialize output value */
		lck->smb2_break.out.oplock_level= lck->smb2_break.in.oplock_level;
		lck->smb2_break.out.reserved	= lck->smb2_break.in.reserved;
		lck->smb2_break.out.reserved2	= lck->smb2_break.in.reserved2;
		lck->smb2_break.out.file	= lck->smb2_break.in.file;
		break;
	}

	/* 
	 * we don't need to call ntvfs_map_async_setup() here,
	 * as lock() doesn't have any output fields
	 */

	return ntvfs->ops->lock(ntvfs, req, lck2);
}


/* 
   NTVFS write generic to any mapper
*/
static NTSTATUS ntvfs_map_write_finish(struct ntvfs_module_context *ntvfs,
				       struct ntvfs_request *req,
				       union smb_write *wr, 
				       union smb_write *wr2, 
				       NTSTATUS status)
{
	union smb_lock *lck;
	union smb_close *cl;
	unsigned int state;

	if (NT_STATUS_IS_ERR(status)) {
		return status;
	}

	switch (wr->generic.level) {
	case RAW_WRITE_WRITE:
		wr->write.out.nwritten    = wr2->generic.out.nwritten;
		break;

	case RAW_WRITE_WRITEUNLOCK:
		wr->writeunlock.out.nwritten = wr2->generic.out.nwritten;

		lck = talloc(wr2, union smb_lock);
		if (lck == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		lck->unlock.level		= RAW_LOCK_UNLOCK;
		lck->unlock.in.file.ntvfs	= wr->writeunlock.in.file.ntvfs;
		lck->unlock.in.count		= wr->writeunlock.in.count;
		lck->unlock.in.offset		= wr->writeunlock.in.offset;

		if (lck->unlock.in.count != 0) {
			/* do the lock sync for now */
			state = req->async_states->state;
			req->async_states->state &= ~NTVFS_ASYNC_STATE_MAY_ASYNC;
			status = ntvfs->ops->lock(ntvfs, req, lck);
			req->async_states->state = state;
		}
		break;

	case RAW_WRITE_WRITECLOSE:
		wr->writeclose.out.nwritten    = wr2->generic.out.nwritten;

		cl = talloc(wr2, union smb_close);
		if (cl == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		cl->close.level		= RAW_CLOSE_CLOSE;
		cl->close.in.file.ntvfs	= wr->writeclose.in.file.ntvfs;
		cl->close.in.write_time	= wr->writeclose.in.mtime;

		if (wr2->generic.in.count != 0) {
			/* do the close sync for now */
			state = req->async_states->state;
			req->async_states->state &= ~NTVFS_ASYNC_STATE_MAY_ASYNC;
			status = ntvfs->ops->close(ntvfs, req, cl);
			req->async_states->state = state;
		}
		break;

	case RAW_WRITE_SPLWRITE:
		break;

	case RAW_WRITE_SMB2:
		wr->smb2.out._pad	= 0;
		wr->smb2.out.nwritten	= wr2->generic.out.nwritten;
		wr->smb2.out.unknown1	= 0;
		break;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	return status;
}


/* 
   NTVFS write generic to any mapper
*/
NTSTATUS ntvfs_map_write(struct ntvfs_module_context *ntvfs,
				  struct ntvfs_request *req,
				  union smb_write *wr)
{
	union smb_write *wr2;
	NTSTATUS status;

	wr2 = talloc(req, union smb_write);
	if (wr2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ntvfs_map_async_setup(ntvfs, req, wr, wr2, 
				       (second_stage_t)ntvfs_map_write_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	wr2->writex.level = RAW_WRITE_GENERIC;

	switch (wr->generic.level) {
	case RAW_WRITE_WRITEX:
		status = NT_STATUS_INVALID_LEVEL;
		break;

	case RAW_WRITE_WRITE:
		wr2->writex.in.file.ntvfs= wr->write.in.file.ntvfs;
		wr2->writex.in.offset    = wr->write.in.offset;
		wr2->writex.in.wmode     = 0;
		wr2->writex.in.remaining = wr->write.in.remaining;
		wr2->writex.in.count     = wr->write.in.count;
		wr2->writex.in.data      = wr->write.in.data;
		status = ntvfs->ops->write(ntvfs, req, wr2);
		break;

	case RAW_WRITE_WRITEUNLOCK:
		wr2->writex.in.file.ntvfs= wr->writeunlock.in.file.ntvfs;
		wr2->writex.in.offset    = wr->writeunlock.in.offset;
		wr2->writex.in.wmode     = 0;
		wr2->writex.in.remaining = wr->writeunlock.in.remaining;
		wr2->writex.in.count     = wr->writeunlock.in.count;
		wr2->writex.in.data      = wr->writeunlock.in.data;
		status = ntvfs->ops->write(ntvfs, req, wr2);
		break;

	case RAW_WRITE_WRITECLOSE:
		wr2->writex.in.file.ntvfs= wr->writeclose.in.file.ntvfs;
		wr2->writex.in.offset    = wr->writeclose.in.offset;
		wr2->writex.in.wmode     = 0;
		wr2->writex.in.remaining = 0;
		wr2->writex.in.count     = wr->writeclose.in.count;
		wr2->writex.in.data      = wr->writeclose.in.data;
		status = ntvfs->ops->write(ntvfs, req, wr2);
		break;

	case RAW_WRITE_SPLWRITE:
		wr2->writex.in.file.ntvfs= wr->splwrite.in.file.ntvfs;
		wr2->writex.in.offset    = 0;
		wr2->writex.in.wmode     = 0;
		wr2->writex.in.remaining = 0;
		wr2->writex.in.count     = wr->splwrite.in.count;
		wr2->writex.in.data      = wr->splwrite.in.data;
		status = ntvfs->ops->write(ntvfs, req, wr2);
		break;

	case RAW_WRITE_SMB2:
		wr2->writex.in.file.ntvfs= wr->smb2.in.file.ntvfs;
		wr2->writex.in.offset    = wr->smb2.in.offset;
		wr2->writex.in.wmode     = 0;
		wr2->writex.in.remaining = 0;
		wr2->writex.in.count     = wr->smb2.in.data.length;
		wr2->writex.in.data      = wr->smb2.in.data.data;
		status = ntvfs->ops->write(ntvfs, req, wr2);
	}

	return ntvfs_map_async_finish(req, status);
}


/* 
   NTVFS read generic to any mapper - finish the out mapping
*/
static NTSTATUS ntvfs_map_read_finish(struct ntvfs_module_context *ntvfs,
				      struct ntvfs_request *req, 
				      union smb_read *rd, 
				      union smb_read *rd2,
				      NTSTATUS status)
{
	switch (rd->generic.level) {
	case RAW_READ_READ:
		rd->read.out.nread	= rd2->generic.out.nread;
		break;
	case RAW_READ_READBRAW:
		rd->readbraw.out.nread	= rd2->generic.out.nread;
		break;
	case RAW_READ_LOCKREAD:
		rd->lockread.out.nread	= rd2->generic.out.nread;
		break;
	case RAW_READ_SMB2:
		rd->smb2.out.data.length= rd2->generic.out.nread;
		rd->smb2.out.remaining	= 0;
		rd->smb2.out.reserved	= 0;
		break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	return status;
}

/* 
   NTVFS read* to readx mapper
*/
NTSTATUS ntvfs_map_read(struct ntvfs_module_context *ntvfs,
				 struct ntvfs_request *req,
				 union smb_read *rd)
{
	union smb_read *rd2;
	union smb_lock *lck;
	NTSTATUS status;
	unsigned int state;

	rd2 = talloc(req, union smb_read);
	if (rd2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = ntvfs_map_async_setup(ntvfs, req, rd, rd2, 
				       (second_stage_t)ntvfs_map_read_finish);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	rd2->readx.level = RAW_READ_READX;
	rd2->readx.in.read_for_execute = false;

	switch (rd->generic.level) {
	case RAW_READ_READX:
		status = NT_STATUS_INVALID_LEVEL;
		break;

	case RAW_READ_READ:
		rd2->readx.in.file.ntvfs= rd->read.in.file.ntvfs;
		rd2->readx.in.offset    = rd->read.in.offset;
		rd2->readx.in.mincnt    = rd->read.in.count;
		rd2->readx.in.maxcnt    = rd->read.in.count;
		rd2->readx.in.remaining = rd->read.in.remaining;
		rd2->readx.out.data     = rd->read.out.data;
		status = ntvfs->ops->read(ntvfs, req, rd2);
		break;

	case RAW_READ_READBRAW:
		rd2->readx.in.file.ntvfs= rd->readbraw.in.file.ntvfs;
		rd2->readx.in.offset    = rd->readbraw.in.offset;
		rd2->readx.in.mincnt    = rd->readbraw.in.mincnt;
		rd2->readx.in.maxcnt    = rd->readbraw.in.maxcnt;
		rd2->readx.in.remaining = 0;
		rd2->readx.out.data     = rd->readbraw.out.data;
		status = ntvfs->ops->read(ntvfs, req, rd2);
		break;

	case RAW_READ_LOCKREAD:
		/* do the initial lock sync for now */
		state = req->async_states->state;
		req->async_states->state &= ~NTVFS_ASYNC_STATE_MAY_ASYNC;

		lck = talloc(rd2, union smb_lock);
		if (lck == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
		lck->lock.level		= RAW_LOCK_LOCK;
		lck->lock.in.file.ntvfs	= rd->lockread.in.file.ntvfs;
		lck->lock.in.count	= rd->lockread.in.count;
		lck->lock.in.offset	= rd->lockread.in.offset;
		status = ntvfs->ops->lock(ntvfs, req, lck);
		req->async_states->state = state;

		rd2->readx.in.file.ntvfs= rd->lockread.in.file.ntvfs;
		rd2->readx.in.offset    = rd->lockread.in.offset;
		rd2->readx.in.mincnt    = rd->lockread.in.count;
		rd2->readx.in.maxcnt    = rd->lockread.in.count;
		rd2->readx.in.remaining = rd->lockread.in.remaining;
		rd2->readx.out.data     = rd->lockread.out.data;

		if (NT_STATUS_IS_OK(status)) {
			status = ntvfs->ops->read(ntvfs, req, rd2);
		}
		break;

	case RAW_READ_SMB2:
		rd2->readx.in.file.ntvfs= rd->smb2.in.file.ntvfs;
		rd2->readx.in.offset    = rd->smb2.in.offset;
		rd2->readx.in.mincnt    = rd->smb2.in.min_count;
		rd2->readx.in.maxcnt    = rd->smb2.in.length;
		rd2->readx.in.remaining = 0;
		rd2->readx.out.data     = rd->smb2.out.data.data;
		status = ntvfs->ops->read(ntvfs, req, rd2);
		break;
	}

done:
	return ntvfs_map_async_finish(req, status);
}


/* 
   NTVFS close generic to any mapper
*/
static NTSTATUS ntvfs_map_close_finish(struct ntvfs_module_context *ntvfs,
					struct ntvfs_request *req,
					union smb_close *cl, 
					union smb_close *cl2, 
					NTSTATUS status)
{
	NT_STATUS_NOT_OK_RETURN(status);

	switch (cl->generic.level) {
	case RAW_CLOSE_SMB2:
		cl->smb2.out.flags        = cl2->generic.out.flags;
		cl->smb2.out._pad         = 0;
		cl->smb2.out.create_time  = cl2->generic.out.create_time;
		cl->smb2.out.access_time  = cl2->generic.out.access_time;
		cl->smb2.out.write_time   = cl2->generic.out.write_time;
		cl->smb2.out.change_time  = cl2->generic.out.change_time;
		cl->smb2.out.alloc_size   = cl2->generic.out.alloc_size;
		cl->smb2.out.size         = cl2->generic.out.size;
		cl->smb2.out.file_attr    = cl2->generic.out.file_attr;
		break;
	default:
		break;
	}

	return status;
}

/* 
   NTVFS close generic to any mapper
*/
NTSTATUS ntvfs_map_close(struct ntvfs_module_context *ntvfs,
				  struct ntvfs_request *req,
				  union smb_close *cl)
{
	union smb_close *cl2;
	NTSTATUS status;

	cl2 = talloc(req, union smb_close);
	if (cl2 == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	switch (cl->generic.level) {
	case RAW_CLOSE_GENERIC:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_CLOSE_CLOSE:
		cl2->generic.level		= RAW_CLOSE_GENERIC;
		cl2->generic.in.file		= cl->close.in.file;
		cl2->generic.in.write_time	= cl->close.in.write_time;
		cl2->generic.in.flags		= 0;
		break;

	case RAW_CLOSE_SPLCLOSE:
		cl2->generic.level		= RAW_CLOSE_GENERIC;
		cl2->generic.in.file		= cl->splclose.in.file;
		cl2->generic.in.write_time	= 0;
		cl2->generic.in.flags		= 0;
		break;

	case RAW_CLOSE_SMB2:
		cl2->generic.level		= RAW_CLOSE_GENERIC;
		cl2->generic.in.file		= cl->smb2.in.file;
		cl2->generic.in.write_time	= 0;
		cl2->generic.in.flags		= cl->smb2.in.flags;
		break;
	}

	status = ntvfs_map_async_setup(ntvfs, req, cl, cl2, 
				       (second_stage_t)ntvfs_map_close_finish);
	NT_STATUS_NOT_OK_RETURN(status);

	status = ntvfs->ops->close(ntvfs, req, cl2);

	return ntvfs_map_async_finish(req, status);
}

/* 
   NTVFS notify generic to any mapper
*/
static NTSTATUS ntvfs_map_notify_finish(struct ntvfs_module_context *ntvfs,
					struct ntvfs_request *req,
					union smb_notify *nt, 
					union smb_notify *nt2, 
					NTSTATUS status)
{
	NT_STATUS_NOT_OK_RETURN(status);

	switch (nt->nttrans.level) {
	case RAW_NOTIFY_SMB2:
		if (nt2->nttrans.out.num_changes == 0) {
			return STATUS_NOTIFY_ENUM_DIR;
		}
		nt->smb2.out.num_changes	= nt2->nttrans.out.num_changes;
		nt->smb2.out.changes		= talloc_steal(req, nt2->nttrans.out.changes);
		break;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	return status;
}


/* 
   NTVFS notify generic to any mapper
*/
NTSTATUS ntvfs_map_notify(struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req,
				   union smb_notify *nt)
{
	union smb_notify *nt2;
	NTSTATUS status;

	nt2 = talloc(req, union smb_notify);
	NT_STATUS_HAVE_NO_MEMORY(nt2);

	status = ntvfs_map_async_setup(ntvfs, req, nt, nt2, 
				       (second_stage_t)ntvfs_map_notify_finish);
	NT_STATUS_NOT_OK_RETURN(status);

	nt2->nttrans.level = RAW_NOTIFY_NTTRANS;

	switch (nt->nttrans.level) {
	case RAW_NOTIFY_NTTRANS:
		status = NT_STATUS_INVALID_LEVEL;
		break;

	case RAW_NOTIFY_SMB2:
		nt2->nttrans.in.file.ntvfs		= nt->smb2.in.file.ntvfs;
		nt2->nttrans.in.buffer_size		= nt->smb2.in.buffer_size;
		nt2->nttrans.in.completion_filter	= nt->smb2.in.completion_filter;
		nt2->nttrans.in.recursive		= nt->smb2.in.recursive;
		status = ntvfs->ops->notify(ntvfs, req, nt2);
		break;
	}

	return ntvfs_map_async_finish(req, status);
}
